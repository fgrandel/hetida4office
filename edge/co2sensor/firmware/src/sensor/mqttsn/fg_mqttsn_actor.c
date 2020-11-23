#include "mqttsn/fg_mqttsn_actor.h"
#include "gpio/fg_gpio.h"
#include "pins.h"
#include <app_scheduler.h>
#include <app_timer.h>
#include <nrf_assert.h>
#include <nrf_atfifo.h>
#include <nrfx.h>

#include <bsp_thread.h>
#include <openthread/thread.h>
#include <thread_utils.h>

#include <mqttsn_client.h>


/** Logging */
#define NRFX_FG_MQTTSN_ACTOR_CONFIG_LOG_ENABLED 1
#define NRFX_FG_MQTTSN_ACTOR_CONFIG_LOG_LEVEL 4
#define NRFX_FG_MQTTSN_ACTOR_CONFIG_INFO_COLOR 7
#define NRFX_FG_MQTTSN_ACTOR_CONFIG_DEBUG_COLOR 7
#define NRFX_LOG_MODULE FG_MQTTSN_ACTOR
#include <nrfx_log.h>


/** Actor resources */
FG_ACTOR_SLOTS_DEC(fg_mqttsn_connect, fg_mqttsn_disconnect, fg_mqttsn_publish);

#define FG_MQTTSN_ACTOR_SLOT_ASSIGNMENT                                                            \
    {                                                                                              \
        [FG_MQTTSN_CONNECT] = fg_mqttsn_connect, [FG_MQTTSN_DISCONNECT] = fg_mqttsn_disconnect,    \
        [FG_MQTTSN_PUBLISH] = fg_mqttsn_publish                                                    \
    }
#define FG_MQTTSN_ACTOR_STATES                                                                     \
    {                                                                                              \
        MQTTSN_UNINITIALIZED, MQTTSN_CONNECTING, MQTTSN_CONNECTED, MQTTSN_PUBLISHING,              \
            MQTTSN_DISCONNECTING, MQTTSN_DISCONNECTED                                              \
    }

#define MAX_CONCURRENT_MESSAGES 10

#define FG_MQTTSN_TASK_INSTANCE_MESSAGE_N(_index, _)                                               \
    CONCAT_2(FG_MQTTSN_TASK_INSTANCE_MESSAGE, _index),
enum
{
    FG_MQTTSN_TASK_INSTANCE_NETWORK,
    MACRO_REPEAT_FOR(MAX_CONCURRENT_MESSAGES, FG_MQTTSN_TASK_INSTANCE_MESSAGE_N)
        FG_MQTTSN_MAX_CONCURRENT_TASKS
};

FG_ACTOR_DEF(
    FG_MQTTSN_ACTOR_SLOT_ASSIGNMENT, FG_MQTTSN_ACTOR_STATES, FG_MQTTSN_MAX_CONCURRENT_TASKS);

FG_ACTOR_TASK_CALLBACKS_DEC(fg_mqttsn_gateway_found_cb, fg_mqttsn_connected_cb,
    fg_mqttsn_disconnected_cb, fg_mqttsn_published_cb);

FG_ACTOR_INTERFACE_LOCAL_DEC();

#define SCHED_QUEUE_SIZE 32 // Maximum number of events in the scheduler queue.
#define SCHED_EVENT_DATA_SIZE APP_TIMER_SCHED_EVENT_DATA_SIZE // Maximum app_scheduler event size.

static otInstance * m_p_fg_mqttsn_thread_instance;

// TODO: Set higher timeouts if possible.
#define DEFAULT_CHILD_TIMEOUT 40 // Thread child timeout [s].
#define DEFAULT_POLL_PERIOD                                                                        \
    1000 // Thread Sleepy End Device polling period when MQTT-SN Asleep. [ms]
#define SHORT_POLL_PERIOD 100 // Thread Sleepy End Device polling period when MQTT-SN Awake. [ms]
#define SEARCH_GATEWAY_TIMEOUT 5 // MQTT-SN Gateway discovery procedure timeout in [s].

static char m_fg_mqttsn_client_id[] =
    "fg_h4o_co2sensor"; // TODO: pass in as parameter to init call.
static mqttsn_client_t m_fg_mqttsn_client;

static mqttsn_connect_opt_t m_fg_mqttsn_connect_opt;

static mqttsn_remote_t m_fg_mqttsn_gateway_addr;
static uint8_t m_fg_mqttsn_gateway_id;

static const char * const m_fg_mqttsn_topic_names[FG_MQTT_TOPIC_NUM] = {"h4o/s1/press",
    "h4o/s1/temp", "h4o/s1/hum", "h4o/s1/co2",
    "h4o/s1/msg"}; // TODO: pass in as parameter to init call.

static mqttsn_topic_t m_fg_mqttsn_topics[FG_MQTT_TOPIC_NUM] = {
    {
        .p_topic_name = m_fg_mqttsn_topic_names[FG_MQTT_TOPIC_PRESSURE],
    },
    {
        .p_topic_name = m_fg_mqttsn_topic_names[FG_MQTT_TOPIC_TEMPERATURE],
    },
    {
        .p_topic_name = m_fg_mqttsn_topic_names[FG_MQTT_TOPIC_HUMIDITY],
    },
    {
        .p_topic_name = m_fg_mqttsn_topic_names[FG_MQTT_TOPIC_CO2],
    },
    {
        .p_topic_name = m_fg_mqttsn_topic_names[FG_MQTT_TOPIC_STATUS],
    }};

static uint16_t m_fg_mqttsn_msg_id = 0;
NRF_ATFIFO_DEF(m_p_fg_mqttsn_message_fifo, fg_mqttsn_message_t *, MAX_CONCURRENT_MESSAGES);
uint8_t m_fg_num_concurrent_messages = 0;


/** Public API */
FG_ACTOR_INIT_DEF(mqttsn, MQTTSN_UNINITIALIZED, MQTTSN_DISCONNECTED, fg_mqttsn_init);


/** Internal implementation */
static void fg_mqttsn_thread_init(void);
static void fg_mqttsn_evt_handler(mqttsn_client_t * p_client, mqttsn_event_t * p_event);
static void fg_mqttsn_connect_opt_init(void);

static void fg_mqttsn_init(void)
{
    NRF_ATFIFO_INIT(m_p_fg_mqttsn_message_fifo);

    APP_SCHED_INIT(SCHED_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);

    LEDS_CONFIGURE(LEDS_MASK);
    bsp_board_leds_off();
    fg_gpio_cfg_out_os_nopull(PIN_MQTTSN_CONNECTION_STATUS);
}

static void fg_mqttsn_sleep(void)
{
    otError error = otLinkSetPollPeriod(thread_ot_instance_get(), DEFAULT_POLL_PERIOD);
    ASSERT(error == OT_ERROR_NONE);
}

static void fg_mqttsn_wake_up(void)
{
    otError error = otLinkSetPollPeriod(thread_ot_instance_get(), SHORT_POLL_PERIOD);
    ASSERT(error == OT_ERROR_NONE);
}

static void fg_mqttsn_state_changed_callback(uint32_t flags, void * p_context);

static void fg_mqttsn_thread_init(void)
{
    thread_configuration_t thread_configuration = {
        .radio_mode = THREAD_RADIO_MODE_RX_OFF_WHEN_IDLE,
        .autocommissioning = true,
        .poll_period = DEFAULT_POLL_PERIOD,
        .default_child_timeout = DEFAULT_CHILD_TIMEOUT,
    };

    thread_init(&thread_configuration);
    thread_cli_init();
    thread_state_changed_callback_set(fg_mqttsn_state_changed_callback);
    APP_ERROR_CHECK(bsp_thread_init(thread_ot_instance_get()));
}

static void fg_mqttsn_connect_opt_init(void)
{
    m_fg_mqttsn_connect_opt.alive_duration = MQTTSN_DEFAULT_ALIVE_DURATION;
    m_fg_mqttsn_connect_opt.clean_session = MQTTSN_DEFAULT_CLEAN_SESSION_FLAG;
    m_fg_mqttsn_connect_opt.will_flag = MQTTSN_DEFAULT_WILL_FLAG;
    m_fg_mqttsn_connect_opt.client_id_len = strlen(m_fg_mqttsn_client_id);
    memcpy(m_fg_mqttsn_connect_opt.p_client_id, (unsigned char *)m_fg_mqttsn_client_id,
        m_fg_mqttsn_connect_opt.client_id_len);
}

FG_ACTOR_SLOT(fg_mqttsn_connect)
{
    ASSERT(!m_p_fg_mqttsn_thread_instance)
    FG_ACTOR_STATE_TRANSITION(MQTTSN_DISCONNECTED, MQTTSN_CONNECTING, "connecting MQTTSN client");

    FG_ACTOR_RUN_TASK(FG_MQTTSN_TASK_INSTANCE_NETWORK, fg_mqttsn_connected_cb);

    APP_ERROR_CHECK(bsp_init(BSP_INIT_LEDS, NULL));

    fg_mqttsn_thread_init();
    m_p_fg_mqttsn_thread_instance = thread_ot_instance_get();

    APP_ERROR_CHECK(mqttsn_client_init(&m_fg_mqttsn_client, MQTTSN_DEFAULT_CLIENT_PORT,
        fg_mqttsn_evt_handler, m_p_fg_mqttsn_thread_instance));
    fg_mqttsn_connect_opt_init();
}

static bool fg_schedule_next_queued_message(
    fg_actor_action_t * p_calling_action, fg_actor_transaction_t * p_next_transaction);

FG_ACTOR_TASK_CALLBACK(fg_mqttsn_connected_cb)
{
    ASSERT(mqttsn_client_state_get(&m_fg_mqttsn_client) == MQTTSN_CLIENT_CONNECTED);
    ASSERT(m_fg_actor_state == MQTTSN_CONNECTED);
}

FG_ACTOR_SLOT(fg_mqttsn_disconnect)
{
    ASSERT(mqttsn_client_state_get(&m_fg_mqttsn_client) == MQTTSN_CLIENT_CONNECTED);
    FG_ACTOR_STATE_TRANSITION(
        MQTTSN_CONNECTED, MQTTSN_DISCONNECTING, "disconnecting MQTTSN client");

    FG_ACTOR_RUN_TASK(FG_MQTTSN_TASK_INSTANCE_NETWORK, fg_mqttsn_disconnected_cb);

    DRVX(mqttsn_client_disconnect(&m_fg_mqttsn_client));
}

FG_ACTOR_TASK_CALLBACK(fg_mqttsn_disconnected_cb)
{
    ASSERT(mqttsn_client_state_get(&m_fg_mqttsn_client) == MQTTSN_CLIENT_DISCONNECTED);
    fg_mqttsn_sleep();
}

static uint8_t fg_mqttsn_get_current_message_task_id() { return m_fg_num_concurrent_messages; }

FG_ACTOR_SLOT(fg_mqttsn_publish)
{
    bool did_schedule_a_message;
    did_schedule_a_message = fg_schedule_next_queued_message(p_calling_action, p_next_transaction);

    if (m_fg_num_concurrent_messages == MAX_CONCURRENT_MESSAGES)
    {
        NRFX_LOG_ERROR("Message buffer is full, dropped message.");
        FG_ACTOR_ERROR(p_calling_action, NRFX_ERROR_NO_MEM);
        return;
    }

    NRFX_LOG_INFO("Queueing message");
    m_fg_num_concurrent_messages++;

    FG_ACTOR_GET_P_ARGS(fg_mqttsn_message_t, p_mqttsn_message, p_calling_action);
    DRVX(nrf_atfifo_alloc_put(
        m_p_fg_mqttsn_message_fifo, &p_mqttsn_message, sizeof(p_mqttsn_message), NULL));

    ASSERT(m_fg_num_concurrent_messages <= MAX_CONCURRENT_MESSAGES);

    if (!did_schedule_a_message)
    {
        fg_schedule_next_queued_message(p_calling_action, p_next_transaction);
    }
}

static bool fg_schedule_next_queued_message(
    fg_actor_action_t * p_calling_action, fg_actor_transaction_t * p_next_transaction)
{
    if (m_fg_num_concurrent_messages == 0)
    {
        return false;
    }

    if (m_fg_actor_state == MQTTSN_CONNECTED)
    {
        FG_ACTOR_STATE_TRANSITION(
            MQTTSN_CONNECTED, MQTTSN_PUBLISHING, "MQTTSN publishing queued message");

        if (p_next_transaction)
            FG_ACTOR_RUN_TASK(fg_mqttsn_get_current_message_task_id(), fg_mqttsn_published_cb);

        const fg_mqttsn_message_t * p_mqttsn_message;
        APP_ERROR_CHECK(nrf_atfifo_get_free(
            m_p_fg_mqttsn_message_fifo, &p_mqttsn_message, sizeof(p_mqttsn_message), NULL));

        ret_code_t err_code = mqttsn_client_publish(&m_fg_mqttsn_client,
            m_fg_mqttsn_topics[p_mqttsn_message->topic_id].topic_id, p_mqttsn_message->p_data,
            p_mqttsn_message->size, &m_fg_mqttsn_msg_id);
        if (err_code != NRFX_SUCCESS)
        {
            NRF_LOG_ERROR("Error while trying to schedule the next queued message");
            if (p_calling_action)
                FG_ACTOR_ERROR(p_calling_action, err_code);
            return false;
        }
        return true;
    }
    else
    {
        // If we're currently re-connecting due to a transient
        // connectivity issue, we'll send the queued messages
        // as soon as we're re-connected to a gateway.
        ASSERT(m_fg_actor_state == MQTTSN_CONNECTING || m_fg_actor_state == MQTTSN_PUBLISHING);
    }
    return false;
}

FG_ACTOR_TASK_CALLBACK(fg_mqttsn_published_cb)
{
    if (m_fg_actor_state == MQTTSN_CONNECTING)
    {
        fg_mqttsn_sleep();
        return;
    }

    if (!fg_schedule_next_queued_message(p_calling_action, p_next_transaction))
    {
        fg_mqttsn_sleep();
    }
}

FG_ACTOR_INTERFACE_DEF(mqttsn, fg_mqttsn_actor_message_code_t)

typedef enum
{
    MQTTSN_INDICATE_CONNECTED,
    MQTTSN_INDICATE_DISCONNECTED
} fg_mqttsn_indicate_connection_status_t;

static void fg_mqttsn_indicate_connection(fg_mqttsn_indicate_connection_status_t indicate)
{
    if (indicate == MQTTSN_INDICATE_CONNECTED)
    {
        nrf_gpio_pin_clear(PIN_MQTTSN_CONNECTION_STATUS);
    }
    else
    {
        nrf_gpio_pin_set(PIN_MQTTSN_CONNECTION_STATUS);
    }
}

static bool fg_mqttsn_finish_task_if_running(
    const uint32_t task_instance_id, const uint32_t error_flags)
{
    if (fg_actor_is_task_running(task_instance_id))
    {
        if (error_flags)
        {
            fg_actor_action_t * p_running_task = fg_actor_get_running_task(task_instance_id);
            FG_ACTOR_ERROR(p_running_task, NRFX_ERROR_TIMEOUT);
        }
        fg_actor_task_finished(task_instance_id);
        return true;
    }
    return false;
}

static void fg_mqttsn_client_reset()
{
    NRFX_LOG_WARNING("Thread connection lost - automatically trying to re-connect.");
    m_fg_actor_state = MQTTSN_CONNECTING;
    fg_mqttsn_indicate_connection(MQTTSN_INDICATE_DISCONNECTED);
    m_fg_mqttsn_gateway_addr = (mqttsn_remote_t){};
    m_fg_mqttsn_gateway_id = 0;
    m_fg_mqttsn_client.client_state = MQTTSN_CLIENT_DISCONNECTED;
    m_fg_mqttsn_client.gateway_discovery.pending = false;
}

static void fg_mqttsn_state_changed_callback(uint32_t flags, void * p_context)
{
    otDeviceRole role = otThreadGetDeviceRole(p_context);
    NRFX_LOG_INFO("State changed! Flags: 0x%08x Current role: %d", flags, role);
    if (flags & OT_CHANGED_THREAD_ROLE)
    {
        if (role >= OT_DEVICE_ROLE_CHILD && m_fg_actor_state == MQTTSN_CONNECTING)
        {
            APP_ERROR_CHECK(
                mqttsn_client_search_gateway(&m_fg_mqttsn_client, SEARCH_GATEWAY_TIMEOUT));
        }

        if (role == OT_DEVICE_ROLE_DETACHED && m_fg_actor_state == MQTTSN_CONNECTED)
        {
            fg_mqttsn_client_reset();
        }
    }
}

static void fg_mqttsn_register_topic(const uint8_t topic_id)
{
    uint32_t err_code =
        mqttsn_client_topic_register(&m_fg_mqttsn_client, m_fg_mqttsn_topic_names[topic_id],
            strlen(m_fg_mqttsn_topic_names[topic_id]), &m_fg_mqttsn_msg_id);
    if (err_code != NRFX_SUCCESS)
    {
        fg_mqttsn_finish_task_if_running(
            FG_MQTTSN_TASK_INSTANCE_NETWORK, err_code); // abort connecting
        fg_mqttsn_client_reset();
        APP_ERROR_CHECK(mqttsn_client_search_gateway(&m_fg_mqttsn_client, SEARCH_GATEWAY_TIMEOUT));
    }
}

static void fg_mqttsn_evt_handler(mqttsn_client_t * p_client, mqttsn_event_t * p_event)
{
    static uint8_t num_registered_topics;
    uint8_t current_message_task_id;

    switch (p_event->event_id)
    {
        case MQTTSN_EVENT_GATEWAY_FOUND:
            NRFX_LOG_INFO("MQTT-SN event: Client has found an active gateway.");
            m_fg_mqttsn_gateway_addr = *(p_event->event_data.connected.p_gateway_addr);
            m_fg_mqttsn_gateway_id = p_event->event_data.connected.gateway_id;
            APP_ERROR_CHECK(mqttsn_client_connect(&m_fg_mqttsn_client, &m_fg_mqttsn_gateway_addr,
                m_fg_mqttsn_gateway_id, &m_fg_mqttsn_connect_opt));
            break;

        case MQTTSN_EVENT_SEARCHGW_TIMEOUT:
            if (p_event->event_data.discovery == 0)
                break;

            NRFX_LOG_ERROR(
                "MQTT-SN event: Gateway discovery result: 0x%x.", p_event->event_data.discovery);
            if (p_event->event_data.discovery != MQTTSN_SEARCH_GATEWAY_FINISHED &&
                otThreadGetDeviceRole(m_p_fg_mqttsn_thread_instance) >= OT_DEVICE_ROLE_CHILD)
            {
                // Continue searching for a gateway...
                APP_ERROR_CHECK(
                    mqttsn_client_search_gateway(&m_fg_mqttsn_client, SEARCH_GATEWAY_TIMEOUT));
            }
            break;

        case MQTTSN_EVENT_CONNECTED:
            NRFX_LOG_INFO("MQTT-SN event: Client connected.");
            num_registered_topics = 0;
            fg_mqttsn_register_topic(num_registered_topics);
            break;

        case MQTTSN_EVENT_REGISTERED:
        {
            const mqttsn_topic_t registered_topic = p_event->event_data.registered.packet.topic;
            NRFX_LOG_INFO(
                "MQTT-SN event: Topic has been registered with ID: %d.", registered_topic.topic_id);

            m_fg_mqttsn_topics[num_registered_topics].topic_id = registered_topic.topic_id;

            num_registered_topics++;
            if (num_registered_topics < FG_MQTT_TOPIC_NUM)
            {
                fg_mqttsn_register_topic(num_registered_topics);
            }
            else
            {
                FG_ACTOR_STATE_TRANSITION(
                    MQTTSN_CONNECTING, MQTTSN_CONNECTED, "MQTTSN client connected");
                fg_mqttsn_indicate_connection(MQTTSN_INDICATE_CONNECTED);
                // connected successfully
                if (!fg_mqttsn_finish_task_if_running(
                        FG_MQTTSN_TASK_INSTANCE_NETWORK, NRFX_SUCCESS))
                {
                    if (!fg_schedule_next_queued_message(NULL, NULL))
                    {
                        fg_mqttsn_sleep();
                    }
                };
            }
            break;
        }

        case MQTTSN_EVENT_PUBLISHED:
            NRFX_LOG_INFO("MQTT-SN event: Client has successfully published content.");
            FG_ACTOR_STATE_TRANSITION(
                MQTTSN_PUBLISHING, MQTTSN_CONNECTED, "MQTTSN queued message published");
            // published successfully
            current_message_task_id = fg_mqttsn_get_current_message_task_id();
            m_fg_num_concurrent_messages--;
            if (!fg_mqttsn_finish_task_if_running(current_message_task_id, NRFX_SUCCESS))
            {
                if (!fg_schedule_next_queued_message(NULL, NULL))
                {
                    fg_mqttsn_sleep();
                }
            };
            break;

        case MQTTSN_EVENT_TIMEOUT:
            NRFX_LOG_ERROR("MQTT-SN event: Dropped message: %d. Message ID: %d due to timeout.",
                p_event->event_data.error.msg_type, p_event->event_data.error.msg_id);
            // abort publishing
            current_message_task_id = fg_mqttsn_get_current_message_task_id();
            m_fg_num_concurrent_messages--;
            fg_mqttsn_finish_task_if_running(
                current_message_task_id, NRFX_ERROR_TIMEOUT);
            fg_mqttsn_client_reset();
            APP_ERROR_CHECK(
                mqttsn_client_search_gateway(&m_fg_mqttsn_client, SEARCH_GATEWAY_TIMEOUT));
            break;

        case MQTTSN_EVENT_DISCONNECTED:
            NRFX_LOG_INFO("MQTT-SN event: Client disconnected.");
            fg_mqttsn_indicate_connection(MQTTSN_INDICATE_DISCONNECTED);
            if (m_fg_actor_state == MQTTSN_DISCONNECTING)
            {
                nrf_atfifo_clear(m_p_fg_mqttsn_message_fifo);
                FG_ACTOR_STATE_TRANSITION(
                    MQTTSN_DISCONNECTING, MQTTSN_DISCONNECTED, "MQTTSN client disconnected");
            }
            else
            {
                m_fg_actor_state = MQTTSN_CONNECTING;
                fg_mqttsn_client_reset();
                APP_ERROR_CHECK(
                    mqttsn_client_search_gateway(&m_fg_mqttsn_client, SEARCH_GATEWAY_TIMEOUT));
            }
            fg_mqttsn_finish_task_if_running(
                FG_MQTTSN_TASK_INSTANCE_NETWORK, NRFX_SUCCESS); // disconnected successfully
            break;

        case MQTTSN_EVENT_SLEEP_PERMIT:
            NRFX_LOG_INFO("MQTT-SN event: Client permitted to sleep.");
            fg_mqttsn_sleep();
            break;

        case MQTTSN_EVENT_SLEEP_STOP:
            NRFX_LOG_INFO("MQTT-SN event: Client wakes up.");
            fg_mqttsn_wake_up();
            break;

        default:
            break;
    }
}