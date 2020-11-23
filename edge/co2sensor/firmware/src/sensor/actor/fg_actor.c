#include "actor/fg_actor.h"
#include <app_util_platform.h>
#include <nrf_balloc.h>
#include <nrf_queue.h>
#include <nrfx.h>
#include <sdk_common.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/** Logging */
#define NRFX_LOG_MODULE FG_ACTOR
#include <nrfx_log.h>


/** Actor Framework resources */
#define MAX_ACTIONS 10
#define MAX_TRANSACTIONS 5

NRF_BALLOC_DEF(m_fg_actor_action_pool, sizeof(fg_actor_action_t), MAX_ACTIONS);

static fg_actor_action_t * const fg_actor_action_alloc_clear(const fg_actor_action_type_t type,
    const fg_actor_t * const p_actor, fg_actor_transaction_t * const p_transaction);
static void fg_actor_action_free(fg_actor_action_t *);

NRF_BALLOC_DEF(m_fg_actor_transaction_pool, sizeof(fg_actor_transaction_t), MAX_TRANSACTIONS);

static fg_actor_transaction_t * const fg_actor_transaction_alloc_clear(
    const fg_actor_action_t * const p_calling_action);
static void fg_actor_transaction_free(fg_actor_transaction_t *);

NRF_QUEUE_DEF(fg_actor_transaction_t *, m_fg_actor_transaction_queue, MAX_TRANSACTIONS,
    NRF_QUEUE_MODE_NO_OVERFLOW);
NRF_QUEUE_DEF(
    fg_actor_action_t *, m_fg_actor_finished_action_queue, MAX_ACTIONS, NRF_QUEUE_MODE_NO_OVERFLOW);


typedef bool (*fg_actor_action_iteratee_t)(
    fg_actor_action_t * const p_action, fg_actor_transaction_t * const p_transaction);
static bool fg_actor_iterate_transaction(
    fg_actor_transaction_t * const p_transaction, const fg_actor_action_iteratee_t iteratee);


void fg_actor_init(void)
{
    nrf_balloc_init(&m_fg_actor_action_pool);
    nrf_balloc_init(&m_fg_actor_transaction_pool);
}

fg_actor_transaction_t * const fg_actor_allocate_root_transaction(
    const fg_actor_result_handler_t result_handler)
{
    // Root transactions (i.e. that are not sub-transactions of another calling
    // action aka the calling action is NULL) must have the result handler always
    // set to guarantee continuity of the program (otherwise the program would stop).
    // This is not necessary for sub-transactions which may not have a result
    // handler but instead their corresponding action is considered done w/o result
    // which calls the calling action's result handler.
    ASSERT(result_handler)
    fg_actor_transaction_t * const p_transaction = fg_actor_transaction_alloc_clear(NULL);
    p_transaction->result_handler = result_handler;
    APP_ERROR_CHECK(nrf_queue_push(&m_fg_actor_transaction_queue, &p_transaction));
    return p_transaction;
}

fg_actor_action_t * const fg_actor_allocate_message(const fg_actor_t * const p_actor,
    const uint32_t code, fg_actor_transaction_t * const p_transaction)
{
    ASSERT(p_actor)
    fg_actor_action_t * const p_action =
        fg_actor_action_alloc_clear(FG_ACTOR_ACTION_TYPE_MESSAGE, p_actor, p_transaction);
    p_action->message.code = code;
    return p_action;
}

fg_actor_action_t * const fg_actor_allocate_task_internal(const fg_actor_t * const p_actor,
    fg_actor_transaction_t * const p_transaction, const uint32_t task_instance_id,
    const fg_actor_task_callback_t task_callback)
{
    ASSERT(p_actor)
    ASSERT(task_callback)
    ASSERT(task_instance_id < p_actor->max_concurrent_tasks)
    ASSERT(!p_actor->running_tasks[task_instance_id])
    fg_actor_action_t * const p_action =
        fg_actor_action_alloc_clear(FG_ACTOR_ACTION_TYPE_TASK, p_actor, p_transaction);
    p_action->task.callback = task_callback;
    p_actor->running_tasks[task_instance_id] = p_action;
    return p_action;
}

bool fg_actor_is_task_running_internal(const fg_actor_t * const p_actor, uint32_t task_instance_id)
{
    ASSERT(p_actor)
    ASSERT(task_instance_id < p_actor->max_concurrent_tasks)
    return p_actor->running_tasks[task_instance_id] != NULL;
}

fg_actor_action_t * const fg_actor_get_running_task_internal(
    const fg_actor_t * const p_actor, uint32_t task_instance_id)
{
    ASSERT(p_actor)
    ASSERT(task_instance_id < p_actor->max_concurrent_tasks)
    fg_actor_action_t * const running_task = p_actor->running_tasks[task_instance_id];
    ASSERT(running_task)
    return running_task;
}

static bool handle_scheduled_transactions(void);
static bool handle_finished_actions(void);

// Must be called from main context.
void fg_actor_run(void)
{
    bool did_work;

    do
    {
        did_work = false;

        // Handle outstanding transactions (which may finish some
        // actions which is why we check finished actions later).
        did_work |= handle_scheduled_transactions();

        // Handle finished actions.
        did_work |= handle_finished_actions();
    } while (did_work);
}

static void fg_actor_run_transaction(fg_actor_transaction_t * const p_transaction);

static bool handle_scheduled_transactions(void)
{
    bool did_work = false;

    fg_actor_transaction_t * p_transaction;
    while (!nrf_queue_is_empty(&m_fg_actor_transaction_queue))
    {
        APP_ERROR_CHECK(nrf_queue_pop(&m_fg_actor_transaction_queue, &p_transaction));

        // Run the next transaction.
        fg_actor_run_transaction(p_transaction);

        did_work = true;
    }

    return did_work;
}

static bool fg_actor_transaction_check_and_handle_done(
    fg_actor_transaction_t * const p_transaction);

static bool handle_finished_actions(void)
{
    bool did_work = false;

    fg_actor_transaction_t * p_transaction;
    const fg_actor_action_t * p_calling_action;
    fg_actor_action_t * p_sub_action;
    fg_actor_task_callback_t task_callback;
    while (!nrf_queue_is_empty(&m_fg_actor_finished_action_queue))
    {
        APP_ERROR_CHECK(nrf_queue_pop(&m_fg_actor_finished_action_queue, &p_sub_action));
        ASSERT(!p_sub_action->is_done)

        p_transaction = p_sub_action->p_transaction;
        ASSERT(p_transaction)
        ASSERT(p_transaction->has_run);

        if (p_sub_action->type == FG_ACTOR_ACTION_TYPE_TASK)
        {
            // Handle the result of finished tasks (which may
            // add results to the sub action or trigger
            // a follow-up task in the transaction, so
            // we need to do this before we check whether the
            // corresponding transaction is now done).
            task_callback = p_sub_action->task.callback;
            ASSERT(task_callback)
            p_calling_action = p_transaction->p_calling_action;
            ASSERT(p_calling_action)
            task_callback(p_transaction, p_calling_action, p_sub_action);
        }

        // The action can now be safely considered "done".
        p_sub_action->is_done = true;

        // Check whether the transaction corresponding to the
        // action has now been finished (i.e. this was the last
        // unfinished task in the transaction) and if so: return
        // the result to its caller.
        fg_actor_transaction_check_and_handle_done(p_transaction);

        did_work = true;
    }

    return did_work;
}

static void fg_actor_post_message(fg_actor_action_t * const p_action);
static bool fg_actor_post_message_iteratee(fg_actor_action_t * const p_action, fg_actor_transaction_t * const p_transaction);
static bool fg_actor_queue_task_if_irq_has_fired_iteratee(fg_actor_action_t * const p_action, fg_actor_transaction_t * const p_transaction);

static void fg_actor_run_transaction(fg_actor_transaction_t * const p_transaction)
{
    ASSERT(p_transaction)
    ASSERT(p_transaction->p_first_concurrent_action)

    // Run all actions within the transaction.
    fg_actor_iterate_transaction(p_transaction, fg_actor_post_message_iteratee);

    CRITICAL_REGION_ENTER();
    fg_actor_iterate_transaction(p_transaction, fg_actor_queue_task_if_irq_has_fired_iteratee);
    ASSERT(!p_transaction->has_run)
    p_transaction->has_run = true;
    CRITICAL_REGION_EXIT();
}

static bool fg_actor_post_message_iteratee(fg_actor_action_t * const p_action, fg_actor_transaction_t * const p_transaction)
{
    // Tasks need not to be scheduled as they are started by
    // the actor itself.
    if (p_action->type == FG_ACTOR_ACTION_TYPE_TASK)
        return true;

    fg_actor_post_message(p_action);
    return true;
}

static bool fg_actor_queue_task_if_irq_has_fired_iteratee(fg_actor_action_t * const p_action, fg_actor_transaction_t * const p_transaction)
{
    // A task may finish (via interrupt) before its corresponding
    // transaction is scheduled and handled. In this case we do not
    // add it to the finished actions queue right away but schedule
    // it once the corresponding transaction is handled (i.e. now),
    // see fg_actor_task_finished().
    if (p_action->task.irq_has_fired)
    {
        APP_ERROR_CHECK(nrf_queue_push(&m_fg_actor_finished_action_queue, &p_action));
    }
    return true;
}

static void fg_actor_queue_or_free_transaction(
    const fg_actor_action_t * const p_action, fg_actor_transaction_t * const p_transaction);

static void fg_actor_post_message(fg_actor_action_t * const p_calling_action)
{
    ASSERT(p_calling_action->type == FG_ACTOR_ACTION_TYPE_MESSAGE)
    ASSERT(!p_calling_action->is_done)

    const fg_actor_t * const p_actor = p_calling_action->p_actor;
    ASSERT(p_actor)

    const uint32_t message_code = p_calling_action->message.code;
    ASSERT(message_code < p_actor->num_slots)

    fg_actor_transaction_t * const p_sub_transaction =
        fg_actor_transaction_alloc_clear(p_calling_action);

    const fg_actor_message_handler_t message_handler = p_actor->slots[message_code];
    ASSERT(message_handler);
    message_handler(p_calling_action, p_sub_transaction);

    fg_actor_queue_or_free_transaction(p_calling_action, p_sub_transaction);
}

static void fg_actor_queue_or_free_transaction(const fg_actor_action_t * const p_calling_action,
    fg_actor_transaction_t * const p_sub_transaction)
{
    // The calling action may be NULL in case of a root transaction
    ASSERT(p_sub_transaction)

    if (p_sub_transaction->p_first_concurrent_action)
    {
        // Another sub-transaction has been requested from the calling action
        // or from root level (i.e. main).
        APP_ERROR_CHECK(nrf_queue_push(&m_fg_actor_transaction_queue, &p_sub_transaction));
    }
    else
    {
        fg_actor_transaction_free(p_sub_transaction);

        // No more sub-transactions means that the calling action is done (if any).
        if (p_calling_action)
            APP_ERROR_CHECK(nrf_queue_push(&m_fg_actor_finished_action_queue, &p_calling_action));
    }
}

// May be called from interrupt context.
// Obs: No need for a critical region here as each task corresponds
// exclusively to one IRQ handler, i.e. one IRQ priority, and therefore
// cannot be interrupted again for the same task.
// There's also no need to synchronize with adtor or transaction state
// as both will only be updated from main context which always runs at
// lower or same priority as this function.
void fg_actor_task_finished_internal(
    const fg_actor_t * const p_actor, const uint32_t task_instance_id)
{
    fg_actor_action_t * const p_task =
        fg_actor_get_running_task_internal(p_actor, task_instance_id);
    p_actor->running_tasks[task_instance_id] = NULL;

    const fg_actor_transaction_t * const p_transaction = p_task->p_transaction;
    ASSERT(p_transaction)

    if (p_transaction->has_run)
    {
        // We push the finished task to an interrupt-safe
        // queue which will be handled from the main context.
        APP_ERROR_CHECK(nrf_queue_push(&m_fg_actor_finished_action_queue, &p_task));
    }
    else
    {
        // This condition occurs when the task's interrupt
        // finished so quickly that the transaction that
        // contains the action did not run yet. In this case
        // the task will be pushed to the finished actions
        // queue when its corresponding transaction has been run,
        // see fg_actor_queue_task_if_irq_has_fired_iteratee().
        NRFX_LOG_DEBUG("Task irq finished before transaction was run.");
    }

    ASSERT(!p_task->task.irq_has_fired)
    p_task->task.irq_has_fired = true;
}

static bool fg_actor_transaction_is_done_iteratee(fg_actor_action_t * const p_action, fg_actor_transaction_t * const p_transaction);
static bool fg_actor_transaction_consolidate_errors_iteratee(fg_actor_action_t * const p_action, fg_actor_transaction_t * const p_transaction);
static void fg_actor_transaction_free_deep(fg_actor_transaction_t * const p_transaction);

static bool fg_actor_transaction_check_and_handle_done(fg_actor_transaction_t * const p_transaction)
{
    ASSERT(p_transaction)
    ASSERT(p_transaction->p_first_concurrent_action)
    ASSERT(p_transaction->has_run)

    bool transaction_is_done = fg_actor_iterate_transaction(
        p_transaction, fg_actor_transaction_is_done_iteratee);
    if (transaction_is_done)
    {
        p_transaction->error_flags = 0;
        fg_actor_iterate_transaction(p_transaction, fg_actor_transaction_consolidate_errors_iteratee);

        const fg_actor_action_t * const p_calling_action = p_transaction->p_calling_action;
        if (p_transaction->result_handler)
        {
            fg_actor_transaction_t * const p_next_transaction =
                fg_actor_transaction_alloc_clear(p_calling_action);
            p_transaction->result_handler(p_transaction, p_next_transaction);
            fg_actor_queue_or_free_transaction(p_calling_action, p_next_transaction);
        }
        else
        {
            // No result handler means that the calling action is considered
            // done with no (i.e. empty) result.
            ASSERT(p_calling_action)
            APP_ERROR_CHECK(nrf_queue_push(&m_fg_actor_finished_action_queue, &p_calling_action));
        }

        fg_actor_transaction_free_deep(p_transaction);
    }
}

static bool fg_actor_transaction_is_done_iteratee(fg_actor_action_t * const p_action, fg_actor_transaction_t * const p_transaction)
{
    return p_action->is_done;
}

static bool fg_actor_transaction_consolidate_errors_iteratee(fg_actor_action_t * const p_action, fg_actor_transaction_t * const p_transaction)
{
    p_transaction->error_flags |= p_action->error_flags;
    return true;
}

static bool fg_actor_transaction_free_deep_iteratee(fg_actor_action_t * const p_action, fg_actor_transaction_t * const p_transaction);

static void fg_actor_transaction_free_deep(fg_actor_transaction_t * const p_transaction)
{
    ASSERT(p_transaction->has_run)

    // Free all actions that were allocated for this transaction.
    fg_actor_iterate_transaction(p_transaction, fg_actor_transaction_free_deep_iteratee);

    // Free the transaction itself.
    fg_actor_transaction_free(p_transaction);
}

static bool fg_actor_transaction_free_deep_iteratee(fg_actor_action_t * const p_action, fg_actor_transaction_t * const p_transaction)
{
    ASSERT(p_action->is_done)
    fg_actor_action_free(p_action);
    return true;
}

NRF_BALLOC_INTERFACE_LOCAL_DEF(
    fg_actor_transaction_t, fg_actor_transaction, &m_fg_actor_transaction_pool)

static fg_actor_transaction_t * const fg_actor_transaction_alloc_clear(
    const fg_actor_action_t * const p_calling_action)
{
    fg_actor_transaction_t * const p_transaction = fg_actor_transaction_alloc();
    ASSERT(p_transaction)
    memset(p_transaction, 0, sizeof(fg_actor_transaction_t));
    p_transaction->p_calling_action = p_calling_action;
    return p_transaction;
}

NRF_BALLOC_INTERFACE_LOCAL_DEF(fg_actor_action_t, fg_actor_action, &m_fg_actor_action_pool)

static void enqueue_action(
    fg_actor_transaction_t * const p_transaction, fg_actor_action_t * const p_action);

static fg_actor_action_t * const fg_actor_action_alloc_clear(const fg_actor_action_type_t type,
    const fg_actor_t * const p_actor, fg_actor_transaction_t * const p_transaction)
{
    fg_actor_action_t * const p_action = fg_actor_action_alloc();
    ASSERT(p_action)
    memset(p_action, 0, sizeof(fg_actor_action_t));

    p_action->type = type;
    p_action->p_actor = p_actor;
    p_action->p_transaction = p_transaction;

    enqueue_action(p_transaction, p_action);

    return p_action;
}

static void enqueue_action(
    fg_actor_transaction_t * const p_transaction, fg_actor_action_t * const p_action)
{
    fg_actor_action_t * p_last_action = p_transaction->p_first_concurrent_action;

    if (p_last_action == NULL)
    {
        // This is the first action of this transaction.
        p_transaction->p_first_concurrent_action = p_action;
        return;
    }

    // There are already other actions - add this action
    // to the end of the queue.
    while (p_last_action->p_next_concurrent_action)
    {
        p_last_action = p_last_action->p_next_concurrent_action;
    }
    p_last_action->p_next_concurrent_action = p_action;
}

static bool fg_actor_iterate_transaction(
    fg_actor_transaction_t * const p_transaction, const fg_actor_action_iteratee_t iteratee)
{
    ASSERT(p_transaction)
    fg_actor_action_t * p_action = p_transaction->p_first_concurrent_action;
    while (p_action)
    {
        if (!iteratee(p_action, p_transaction))
            return false;
        p_action = p_action->p_next_concurrent_action;
    }
    return true;
}