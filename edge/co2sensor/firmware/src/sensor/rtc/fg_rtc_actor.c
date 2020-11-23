#include "fg_rtc_actor.h"
#include <app_timer.h>
#include <app_util_platform.h>
#include <nrfx.h>

/** Logging */
#define NRFX_FG_RTC_ACTOR_CONFIG_LOG_ENABLED 0
#define NRFX_FG_RTC_ACTOR_CONFIG_LOG_LEVEL 4
#define NRFX_FG_RTC_ACTOR_CONFIG_INFO_COLOR 3
#define NRFX_FG_RTC_ACTOR_CONFIG_DEBUG_COLOR 3
#define NRFX_LOG_MODULE FG_RTC_ACTOR
#include <nrfx_log.h>


/** Actor resources */
FG_ACTOR_SLOTS_DEC(fg_rtc_start_timer);

#define FG_RTC_ACTOR_SLOT_ASSIGNMENT                                                               \
    {                                                                                              \
        [FG_RTC_START_TIMER] = fg_rtc_start_timer                                                  \
    }
#define FG_RTC_ACTOR_STATES                                                                        \
    {                                                                                              \
        RTC_UNINITIALIZED, RTC_IDLE, RTC_RUNNING                                                   \
    }
FG_ACTOR_DEF(FG_RTC_ACTOR_SLOT_ASSIGNMENT, FG_RTC_ACTOR_STATES, FG_ACTOR_SINGLETON_TASK);

FG_ACTOR_TASK_CALLBACKS_DEC(fg_rtc_timeout_cb);

FG_ACTOR_INTERFACE_LOCAL_DEC();

APP_TIMER_DEF(m_fg_rtc_timer_id);


/** Public API */
FG_ACTOR_INIT_DEF(rtc, RTC_UNINITIALIZED, RTC_IDLE, fg_rtc_init);


/** Internal implementation */
static void fg_rtc_timeout_handler();

static void fg_rtc_init(void)
{
    APP_ERROR_CHECK(app_timer_init());

    APP_ERROR_CHECK(
        app_timer_create(&m_fg_rtc_timer_id, APP_TIMER_MODE_SINGLE_SHOT, fg_rtc_timeout_handler));
}

FG_ACTOR_SLOT(fg_rtc_start_timer)
{
    FG_ACTOR_STATE_TRANSITION(RTC_IDLE, RTC_RUNNING, "starting RTC timer");

    FG_ACTOR_RUN_SINGLETON_TASK(fg_rtc_timeout_cb);

    // Get the timeout from the message.
    FG_ACTOR_GET_ARGS(uint32_t, timeout, p_calling_action); // in ms.
    uint32_t ticks = APP_TIMER_TICKS(timeout);

    // Make sure the timeout is within reachable limits.
    ASSERT(ticks >= APP_TIMER_MIN_TIMEOUT_TICKS)

    app_timer_start(m_fg_rtc_timer_id, ticks, NULL);
}

FG_ACTOR_TASK_CALLBACK(fg_rtc_timeout_cb)
{
    FG_ACTOR_STATE_TRANSITION(RTC_RUNNING, RTC_IDLE, "RTC timer fired");
}

FG_ACTOR_INTERFACE_DEF(rtc, fg_rtc_actor_message_code_t)

static void fg_rtc_timeout_handler()
{
    if (m_fg_actor_state != RTC_RUNNING)
        return;

    FG_ACTOR_SINGLETON_TASK_FINISHED();
}