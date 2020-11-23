#ifndef FG_ACTOR_H__
#define FG_ACTOR_H__

#include <app_util.h>
#include <stdint.h>
#include <stdlib.h>

// Actor spec:
// - Transactions can be scheduled from main or interrupt context but the corresponding action
//   handlers will always be called from main context.
// - Sending messages to actors or scheduling tasks returns right away, messages will be
//   scheduled asynchronously.
// - An actor must not share any memory with any other actor except for well-defined
//   message-related resources. This will be enforced by the framework.
// - Transaction result handlers will always be called from main context.
// - Actors may handle several transactions in parallel, result handlers therefore receive
//   a reference to the transaction they refer to.

// TODO: implement improved reset behavior
// - When resetting an actor with running sub-transactions then:
//    1) cancel all running sub-transactions
//    2) make sure sub-transactions already being processed will not have any effect when returning


// Public data structures
typedef struct fg_actor fg_actor_t;

typedef struct fg_actor_transaction fg_actor_transaction_t;

typedef struct fg_actor_action fg_actor_action_t;

typedef struct fg_actor_message fg_actor_message_t;

typedef struct fg_actor_task fg_actor_task_t;

typedef enum
{
    FG_ACTOR_ACTION_TYPE_MESSAGE, // an async message to a sub-actor
    FG_ACTOR_ACTION_TYPE_TASK     // an async hardware task
} fg_actor_action_type_t;

#define FG_ACTOR_TASK_CALLBACK_ARGS_DEC                                                            \
    fg_actor_transaction_t *const p_next_transaction,                                              \
        fg_actor_action_t *const p_calling_action,                                           \
        fg_actor_action_t *const p_completed_action
#define FG_ACTOR_TASK_CALLBACK_ARGS p_next_transaction, p_calling_action, p_completed_action
typedef void (*fg_actor_task_callback_t)(FG_ACTOR_TASK_CALLBACK_ARGS_DEC);

// Represents an actor-internal asynchronous (e.g. hardware) task
// that will fire an event signaled via IRQ. Each task instance
// within an actor must correspond to exactly one interrupt as two
// instances of the same task cannot be scheduled concurrently.
struct fg_actor_task
{
    fg_actor_task_callback_t callback; // the event listener assigned to the task instance
    volatile bool irq_has_fired;       // set to true within ISR context - used for interrupt-safe
                                       // synchronization with the main context
};

// Represents a message from main context or an actor
// to a public slot of a (sub)actor.
struct fg_actor_message
{
    uint32_t code; // must be unique integers within an actor (zero-based, gap-free)
};

// Represents any asynchronous action that needs to be
// executed from the event loop.
// Can be either an asynchronous actor-internal task
// or an externally visible actor message slot.
struct fg_actor_action
{
    fg_actor_action_type_t type; // external message or internal task
    union
    { // type specific fields
        fg_actor_message_t message;
        fg_actor_task_t task;
    };
    const void * p_args; // arguments passed in to the action
    size_t args_size;
    const fg_actor_t * p_actor;
    void * p_result; // results produced by the action
    size_t result_size;
    uint32_t error_flags; // error mask representing errors occurred while processing the action
    fg_actor_transaction_t * p_transaction;       // the transaction bundling this action
    fg_actor_action_t * p_next_concurrent_action; // link to the next action in this transaction
    bool is_done; // true once an action's message/event handler has been called
};

#define FG_ACTOR_RESULT_HANDLER_ARGS_DEC                                                           \
    const fg_actor_transaction_t *const p_completed_transaction,                                   \
        fg_actor_transaction_t *const p_next_transaction
#define FG_ACTOR_RESULT_HANDLER_ARGS p_completed_transaction, p_next_transaction
typedef void (*fg_actor_result_handler_t)(FG_ACTOR_RESULT_HANDLER_ARGS_DEC);

// Represents a number of concurrent asynchronous actions
// and a corresponding synchronization point: Execution
// of sequential transactions will only be started once
// all actions in the previous transaction have finished.
struct fg_actor_transaction
{
    fg_actor_result_handler_t
        result_handler; // result handler to be executed once the transaction is considered "done"
    const fg_actor_action_t * p_calling_action; // Action context in which this transaction was
                                                // created - may be NULL in the case of top-level
                                                // root transactions.
    fg_actor_action_t *
        p_first_concurrent_action; // pointer to the first task or message in this transaction
    uint32_t error_flags; // error mask representing errors occurred while processing any of the
                          // contained actions
    bool has_run; // whether all message handlers of messages in this transaction have been called
    // Obs: A transaction is considered "run" once all
    // message handlers in this transaction have been
    // executed (not necessarily the event handlers, though).
    // It is considered "done" when all event handlers
    // have been executed, too, i.e. when all linked
    // messages and tasks are in a "done" state.
};

#define FG_ACTOR_MESSAGE_HANDLER_ARGS_DEC                                                          \
    fg_actor_action_t *const p_calling_action, fg_actor_transaction_t *const p_next_transaction
#define FG_ACTOR_MESSAGE_HANDLER_ARGS p_calling_action, p_next_transaction
typedef void (*fg_actor_message_handler_t)(FG_ACTOR_MESSAGE_HANDLER_ARGS_DEC);

// Some predefined task configurations for ...
// ... actors that use only one concurrent hardware task
enum fg_actor_task_singleton
{
    FG_ACTOR_DEFAULT_TASK_INSTANCE,
    FG_ACTOR_SINGLETON_TASK // used as max concurrent tasks number
};
// ... actors that use no own hardware tasks (only sub-actors)
enum fg_actor_task_none
{
    FG_ACTOR_NO_TASKS // used as max concurrent tasks number
};

// Represents an autonomous actor with a number of
// public slots (i.e. message endpoints). Messages
// sent to one of these endpoints will be handled
// asynchronously.
struct fg_actor
{
    // Maps slot ids to message handlers
    const fg_actor_message_handler_t * const slots;
    // Num of slots/message handlers
    const size_t num_slots;
    // A (dynamically updated) list of currently running tasks
    // with "max_concurrent_tasks" number of entries. Unused
    // task pointers must be set to NULL.
    // The actor implementation is responsible for assigning
    // unique integer IDs to task instances.
    // The framework will check that a task instance slot is
    // free before assigning a task to it.
    fg_actor_action_t ** const running_tasks;
    // Max number of tasks that can run in parallel.
    const size_t max_concurrent_tasks;
};


// Private/local actor resources and interface.
#define FG_ACTOR_DEF(_slots, _states, _max_concurrent_tasks)                                       \
    static const fg_actor_message_handler_t m_fg_actor_slots[] = _slots;                           \
    static fg_actor_action_t * m_p_fg_actor_running_tasks[_max_concurrent_tasks];                  \
    static const fg_actor_t m_fg_actor = {.slots = m_fg_actor_slots,                               \
        .num_slots = sizeof(m_fg_actor_slots) / sizeof(m_fg_actor_slots[0]),                       \
        .running_tasks = m_p_fg_actor_running_tasks,                                               \
        .max_concurrent_tasks = (_max_concurrent_tasks)};                                          \
    typedef enum _states fg_actor_state_t;                                                         \
    static fg_actor_state_t m_fg_actor_state

#define FG_ACTOR_INTERFACE_DEC(_actor_name, _message_code_enum)                                    \
    const fg_actor_t * const fg_##_actor_name##_actor_init(void);                                  \
    fg_actor_action_t * const fg_##_actor_name##_actor_allocate_message(                           \
        _message_code_enum code, fg_actor_transaction_t * const p_transaction)

#define FG_ACTOR_INTERFACE_LOCAL_DEC()                                                             \
    static const fg_actor_t * const fg_actor_get(void);                                            \
    static fg_actor_action_t * const fg_actor_allocate_task(                                       \
        fg_actor_transaction_t * const p_transaction, const uint32_t task_instance_id,             \
        const fg_actor_task_callback_t task_callback);                                             \
    static bool fg_actor_is_task_running(const uint32_t task_instance_id);                         \
    static fg_actor_action_t * const fg_actor_get_running_task(const uint32_t task_instance_id);   \
    static void fg_actor_task_finished(const uint32_t task_instance_id)

#define FG_ACTOR_INIT_DEF(_actor_name, _init_state, _off_state, _once_per_actor_init_func)         \
    static void _once_per_actor_init_func(void);                                                   \
    const fg_actor_t * const fg_##_actor_name##_actor_init(void)                                   \
    {                                                                                              \
        ASSERT(m_fg_actor_state <= (_off_state))                                                   \
        if (m_fg_actor_state == (_init_state))                                                     \
        {                                                                                          \
            _once_per_actor_init_func();                                                           \
            m_fg_actor_state = (_off_state);                                                       \
        }                                                                                          \
        return fg_actor_get();                                                                     \
    }

#define FG_ACTOR_INTERFACE_DEF(_actor_name, _message_code_enum)                                    \
    static const fg_actor_t * const fg_actor_get(void) { return &m_fg_actor; }                     \
    fg_actor_action_t * const fg_##_actor_name##_actor_allocate_message(                           \
        _message_code_enum code, fg_actor_transaction_t * const p_transaction)                     \
    {                                                                                              \
        return fg_actor_allocate_message(&m_fg_actor, code, p_transaction);                        \
    }                                                                                              \
    static fg_actor_action_t * const fg_actor_allocate_task(                                       \
        fg_actor_transaction_t * const p_transaction, const uint32_t task_instance_id,             \
        const fg_actor_task_callback_t task_callback)                                              \
    {                                                                                              \
        return fg_actor_allocate_task_internal(                                                    \
            &m_fg_actor, p_transaction, task_instance_id, task_callback);                          \
    }                                                                                              \
    static bool fg_actor_is_task_running(const uint32_t task_instance_id)                          \
    {                                                                                              \
        return fg_actor_is_task_running_internal(&m_fg_actor, task_instance_id);                   \
    }                                                                                              \
    static fg_actor_action_t * const fg_actor_get_running_task(const uint32_t task_instance_id)    \
    {                                                                                              \
        return fg_actor_get_running_task_internal(&m_fg_actor, task_instance_id);                  \
    }                                                                                              \
    static void fg_actor_task_finished(const uint32_t task_instance_id)                            \
    {                                                                                              \
        return fg_actor_task_finished_internal(&m_fg_actor, task_instance_id);                     \
    }

#define FG_ACTOR_SLOT(_name) static void _name(FG_ACTOR_MESSAGE_HANDLER_ARGS_DEC)
#define FG_ACTOR_SLOT_DEC(_name) FG_ACTOR_SLOT(_name);
#define FG_ACTOR_SLOTS_DEC(...) MACRO_MAP(FG_ACTOR_SLOT_DEC, __VA_ARGS__)

#define FG_ACTOR_TASK_CALLBACK(_name) static void _name(FG_ACTOR_TASK_CALLBACK_ARGS_DEC)
#define FG_ACTOR_TASK_CALLBACK_DEC(_name) FG_ACTOR_TASK_CALLBACK(_name);
#define FG_ACTOR_TASK_CALLBACKS_DEC(...) MACRO_MAP(FG_ACTOR_TASK_CALLBACK_DEC, __VA_ARGS__)

#define FG_ACTOR_RESULT_HANDLER(_name) static void _name(FG_ACTOR_RESULT_HANDLER_ARGS_DEC)
#define FG_ACTOR_RESULT_HANDLER_DEC(_name) FG_ACTOR_RESULT_HANDLER(_name);
#define FG_ACTOR_RESULT_HANDLERS_DEC(...) MACRO_MAP(FG_ACTOR_RESULT_HANDLER_DEC, __VA_ARGS__)

#define FG_ACTOR_POST_MESSAGE(_actor_name, _slot_name)                                             \
    fg_##_actor_name##_actor_allocate_message((_slot_name), p_next_transaction)

#define FG_ACTOR_RUN_TASK(_task_instance_id, _task_callback)                                       \
    fg_actor_allocate_task(p_next_transaction, (_task_instance_id), (_task_callback))

#define FG_ACTOR_RUN_SINGLETON_TASK(_task_callback)                                                \
    FG_ACTOR_RUN_TASK(FG_ACTOR_DEFAULT_TASK_INSTANCE, (_task_callback))

#define FG_ACTOR_SINGLETON_TASK_FINISHED() fg_actor_task_finished(FG_ACTOR_DEFAULT_TASK_INSTANCE)

#define FG_ACTOR_GET_SINGLETON_TASK() fg_actor_get_running_task(FG_ACTOR_DEFAULT_TASK_INSTANCE)

#define FG_ACTOR_IS_SINGLETON_TASK_RUNNING()                                                       \
    fg_actor_is_task_running(FG_ACTOR_DEFAULT_TASK_INSTANCE)

#define FG_ACTOR_GET_FIRST_COMPLETED_ACTION() (p_completed_transaction->p_first_concurrent_action)

#define FG_ACTOR_SET_P_X_(_x, _p_action, _type, _p_var)                                            \
    (_p_action)->p_##_x = (_p_var);                                                                \
    (_p_action)->_x##_size = sizeof(_type)

#define FG_ACTOR_SET_X_(_x, _p_action, _var)                                                       \
    (_p_action)->p_##_x = &(_var);                                                                 \
    (_p_action)->_x##_size = sizeof(_var)

#define FG_ACTOR_SET_P_ARGS(_p_action, _type, _p_args)                                             \
    FG_ACTOR_SET_P_X_(args, (_p_action), _type, (_p_args))

#define FG_ACTOR_SET_ARGS(_p_action, _args) FG_ACTOR_SET_X_(args, (_p_action), (_args))

#define FG_ACTOR_SET_P_RESULT(_p_action, _type, _p_result)                                         \
    FG_ACTOR_SET_P_X_(result, (_p_action), _type, (_p_result))

#define FG_ACTOR_SET_RESULT(_p_action, _result) FG_ACTOR_SET_X_(result, (_p_action), (_result))

#define FG_ACTOR_GET_P_X_(_x, _type, _p_target, _p_action)                                         \
    ASSERT((_p_action)->_x##_size == sizeof(_type));                                               \
    const _type * const _p_target = (_p_action)->p_##_x

#define FG_ACTOR_GET_X_(_x, _type, _target, _p_action)                                             \
    ASSERT((_p_action)->_x##_size == sizeof(_type))                                                \
    const _type _target = *(_type *)(_p_action)->p_##_x

#define FG_ACTOR_GET_P_ARGS(_type, _p_target, _p_action)                                           \
    FG_ACTOR_GET_P_X_(args, _type, _p_target, (_p_action))

#define FG_ACTOR_GET_ARGS(_type, _target, _p_action)                                               \
    FG_ACTOR_GET_X_(args, _type, _target, (_p_action))

#define FG_ACTOR_GET_P_RESULT(_type, _p_target, _p_action)                                         \
    FG_ACTOR_GET_P_X_(result, _type, _p_target, (_p_action))

#define FG_ACTOR_GET_P_RESULT_ARR(_element_type, _num_elements, _target, _p_action)                \
    ASSERT((_p_action)->result_size == sizeof(_element_type[_num_elements]));                      \
    const _element_type * const _target = (_p_action)->p_result

#define FG_ACTOR_GET_RESULT(_type, _target, _p_action)                                             \
    FG_ACTOR_GET_X_(result, _type, _target, (_p_action))

#define FG_ACTOR_SET_TRANSACTION_RESULT_HANDLER(_result_handler)                                   \
    p_next_transaction->result_handler = (_result_handler)


// Public actor framework interface.
void fg_actor_init(void);

void fg_actor_run(void);

fg_actor_transaction_t * const fg_actor_allocate_root_transaction(
    const fg_actor_result_handler_t result_handler);

fg_actor_action_t * const fg_actor_allocate_message(const fg_actor_t * const p_actor,
    const uint32_t code, fg_actor_transaction_t * const p_transaction);

fg_actor_action_t * const fg_actor_allocate_task_internal(const fg_actor_t * const p_actor,
    fg_actor_transaction_t * const p_transaction, const uint32_t task_instance_id,
    const fg_actor_task_callback_t task_callback);

bool fg_actor_is_task_running_internal(
    const fg_actor_t * const p_actor, const uint32_t task_instance_id);

fg_actor_action_t * const fg_actor_get_running_task_internal(
    const fg_actor_t * const p_actor, const uint32_t task_instance_id);

void fg_actor_task_finished_internal(
    const fg_actor_t * const p_actor, const uint32_t task_instance_id);


// Error handling support
#define FG_ACTOR_TX_DONE NULL
#define FG_ACTOR_ERROR(_p_action, _nrfx_result)                                                    \
    ((_p_action)->error_flags |= (1L << (_nrfx_result - 1)))

#define DRVX(_nrfx_result)                                                                         \
    do                                                                                             \
    {                                                                                              \
        if ((_nrfx_result) != NRFX_SUCCESS)                                                        \
        {                                                                                          \
            FG_ACTOR_ERROR(p_calling_action, (_nrfx_result));                                      \
            return;                                                                                \
        }                                                                                          \
    } while (0)

#define CHECK_COMPLETED_ACTION(_p_completed_action, _error_message)                                \
    do                                                                                             \
    {                                                                                              \
        if ((_p_completed_action)->error_flags)                                                    \
        {                                                                                          \
            NRF_LOG_ERROR(_error_message);                                                         \
            return;                                                                                \
        }                                                                                          \
    } while (0)

#define RESET_ON_ERROR(_reset_handler, _error_message)                                             \
    do                                                                                             \
    {                                                                                              \
        if (p_completed_transaction->error_flags)                                                  \
        {                                                                                          \
            NRF_LOG_ERROR((_error_message), p_completed_transaction->error_flags);                 \
            _reset_handler(FG_ACTOR_RESULT_HANDLER_ARGS);                                          \
            return;                                                                                \
        }                                                                                          \
    } while (0)


// Actor state handling support
#define FG_ACTOR_STATE_TRANSITION(_expected_state, _next_state, _next_state_name)                  \
    ASSERT(m_fg_actor_state == (_expected_state))                                                  \
    m_fg_actor_state = (_next_state);                                                              \
    NRFX_LOG_DEBUG(_next_state_name)


#endif // FG_ACTOR_H__