#include "bme280/fg_bme280_actor.h"
#include "lp8/fg_lp8_actor.h"
#include "mqttsn/fg_mqttsn_actor.h"
#include "rtc/fg_rtc_actor.h"
#include <app_scheduler.h>
#include <nrf_assert.h>
#include <nrf_log.h>
#include <nrf_log_ctrl.h>
#include <nrf_log_default_backends.h>
#include <stdlib.h>
#include <thread_utils.h>

// Time between measurements
#define MAIN_IDLE_TIME_MS (16000) // min 16s (min time between LP8 measurements), currently 16s

FG_ACTOR_RESULT_HANDLERS_DEC(init_or_wakeup_result_handler, pressure_measurement_result_handler,
    co2_measurement_result_handler);

/** BME280 child actor resources */
static const fg_actor_t * m_p_bme280_actor;

/** LP8 child actor resources */
static const fg_actor_t * m_p_lp8_actor;

/** RTC child actor resources */
static const fg_actor_t * m_p_rtc_actor;
static const uint32_t m_main_idle_time = MAIN_IDLE_TIME_MS;

/** MQTTSN child actor resources */
static const fg_actor_t * m_p_mqttsn_actor;
static char m_mqttsn_message_buffer[FG_MQTT_TOPIC_NUM][20];

static fg_mqttsn_message_t m_mqttsn_message[FG_MQTT_TOPIC_NUM] = {
  {.p_data = m_mqttsn_message_buffer[FG_MQTT_TOPIC_PRESSURE]},
  {.p_data = m_mqttsn_message_buffer[FG_MQTT_TOPIC_TEMPERATURE]},
  {.p_data = m_mqttsn_message_buffer[FG_MQTT_TOPIC_HUMIDITY]},
  {.p_data = m_mqttsn_message_buffer[FG_MQTT_TOPIC_CO2]},
  {.p_data = m_mqttsn_message_buffer[FG_MQTT_TOPIC_STATUS]},
};


/** Main loop */

// TODO: connect gpio pin to sleep state and check whether the device is sleeping as intended.
// TODO: Make sure EN_BLK_REVERSE is Hi-Z during reset to make sure that VCAP will not
//       (reverse) aliment 3V3.
// TODO: implement watchdog (capture ASSERTS and show it somehow e.g. via buzzer), stack guard, mpu,
//       NRFX_PRS
// TODO: implement a keep-alive LED that shortly flashes whenever a keep-alive packet is sent out
// TODO: save sensor state in non-volatile memory after re-calibration and
//       re-load it after reset (verify first if calibration data is not saved on
//       chip anyway).
// TODO: install and use DC/DC regulator
// TODO: implement low-power detection together with a low-power buzzer
// TODO: use the buzzer also to indicate dangerously high levels of CO2 independently
int main(void)
{
    APP_ERROR_CHECK(NRF_LOG_INIT(NULL));
    NRF_LOG_DEFAULT_BACKENDS_INIT();
    NRF_LOG_INFO("FG sensor app started.");

    fg_actor_init();

    m_p_rtc_actor = fg_rtc_actor_init(); // Starts the RTC clock - so must be called before
                                         // any other actor requiring RTC service.
    m_p_mqttsn_actor = fg_mqttsn_actor_init();
    m_p_bme280_actor = fg_bme280_actor_init();
    m_p_lp8_actor = fg_lp8_actor_init();

    fg_actor_transaction_t * const p_init_transaction =
        fg_actor_allocate_root_transaction(init_or_wakeup_result_handler);
    fg_mqttsn_actor_allocate_message(FG_MQTTSN_CONNECT, p_init_transaction);
    fg_bme280_actor_allocate_message(FG_BME280_RESET, p_init_transaction);

    while (true)
    {
        fg_actor_run();
        thread_process();
        app_sched_execute();

        if (NRF_LOG_PROCESS() == false)
        {
            thread_sleep();
        }
    }
}


static void check_publish_action(const fg_actor_action_t * const p_completed_action)
{
    ASSERT(p_completed_action != NULL);
    ASSERT(p_completed_action->p_actor == m_p_mqttsn_actor)
    ASSERT(p_completed_action->message.code == FG_MQTTSN_PUBLISH)
    CHECK_COMPLETED_ACTION(p_completed_action, "MQTT publishing error!");
}

/** Implementation */
FG_ACTOR_RESULT_HANDLER(init_or_wakeup_result_handler)
{
    static fg_bme280_measurement_t bme280_measurement;

    const fg_actor_action_t * p_completed_action = FG_ACTOR_GET_FIRST_COMPLETED_ACTION();
    if (p_completed_action->p_actor == m_p_mqttsn_actor)
    {
        ASSERT(p_completed_action->message.code == FG_MQTTSN_CONNECT)
        CHECK_COMPLETED_ACTION(p_completed_action, "MQTTSN gateway not found!");

        p_completed_action = p_completed_action->p_next_concurrent_action;
        ASSERT(p_completed_action != NULL);
        ASSERT(p_completed_action->p_actor == m_p_bme280_actor)
        ASSERT(p_completed_action->message.code == FG_BME280_RESET)
        CHECK_COMPLETED_ACTION(p_completed_action, "BME280 initialization error!");
    }
    else if (p_completed_action->p_actor == m_p_rtc_actor)
    {
        ASSERT(p_completed_action->message.code == FG_RTC_START_TIMER);
        CHECK_COMPLETED_ACTION(p_completed_action, "RTC error!");

        check_publish_action(p_completed_action->p_next_concurrent_action);
    }
    else
    {
        // Unexpected actor.
        ASSERT(false)
    }

    // Start measuring.
    fg_actor_action_t * const p_next_action = FG_ACTOR_POST_MESSAGE(bme280, FG_BME280_MEASURE);
    FG_ACTOR_SET_P_RESULT(p_next_action, fg_bme280_measurement_t, &bme280_measurement);
    FG_ACTOR_SET_TRANSACTION_RESULT_HANDLER(pressure_measurement_result_handler);
}

static void publish_measurement(
    fg_actor_transaction_t * const p_next_transaction, int32_t measurement, uint16_t topic_id)
{
    snprintf(m_mqttsn_message_buffer[topic_id], sizeof(m_mqttsn_message_buffer[topic_id]), "%d", measurement);
    m_mqttsn_message[topic_id].size = strlen(m_mqttsn_message_buffer[topic_id]);
    ASSERT(m_mqttsn_message[topic_id].size <= sizeof(m_mqttsn_message_buffer[topic_id]));
    m_mqttsn_message[topic_id].topic_id = topic_id;

    fg_actor_action_t * p_next_action = FG_ACTOR_POST_MESSAGE(mqttsn, FG_MQTTSN_PUBLISH);
    FG_ACTOR_SET_ARGS(p_next_action, m_mqttsn_message[topic_id]);
}

FG_ACTOR_RESULT_HANDLER(pressure_measurement_result_handler)
{
    static uint16_t pressure;
    static fg_lp8_measurement_t lp8_measurement;

    const fg_actor_action_t * const p_completed_action = FG_ACTOR_GET_FIRST_COMPLETED_ACTION();
    CHECK_COMPLETED_ACTION(p_completed_action, "BME280 measurement error!");

    ASSERT(p_completed_action->p_actor == m_p_bme280_actor)
    ASSERT(p_completed_action->message.code == FG_BME280_MEASURE)

    FG_ACTOR_GET_P_RESULT(fg_bme280_measurement_t, p_measurement_result, p_completed_action);
    NRF_LOG_INFO("Measurement: pressure %u, temperature %d, humidity %u\n",
        p_measurement_result->pressure, p_measurement_result->temperature,
        p_measurement_result->humidity);

    // BME280 returns pressure in units of 1/100Pa, LP8 expects units of 10Pa = 0.1hPa
    fg_actor_action_t * const p_next_action = FG_ACTOR_POST_MESSAGE(lp8, FG_LP8_MEASURE);
    pressure = p_measurement_result->pressure / 1000;
    FG_ACTOR_SET_ARGS(p_next_action, pressure);

    FG_ACTOR_SET_P_RESULT(p_next_action, fg_lp8_measurement_t, &lp8_measurement);

    publish_measurement(
        p_next_transaction, p_measurement_result->temperature, FG_MQTT_TOPIC_TEMPERATURE);
    publish_measurement(p_next_transaction, p_measurement_result->humidity, FG_MQTT_TOPIC_HUMIDITY);
    publish_measurement(p_next_transaction, p_measurement_result->pressure, FG_MQTT_TOPIC_PRESSURE);

    FG_ACTOR_SET_TRANSACTION_RESULT_HANDLER(co2_measurement_result_handler);
}

FG_ACTOR_RESULT_HANDLER(co2_measurement_result_handler)
{
    const fg_actor_action_t * const p_completed_action = FG_ACTOR_GET_FIRST_COMPLETED_ACTION();
    CHECK_COMPLETED_ACTION(p_completed_action, "LP8 measurement error!");

    ASSERT(p_completed_action->p_actor == m_p_lp8_actor)
    ASSERT(p_completed_action->message.code == FG_LP8_MEASURE)

    fg_actor_action_t * publish_action = p_completed_action->p_next_concurrent_action;
    while (publish_action)
    {
        check_publish_action(publish_action);
        publish_action = publish_action->p_next_concurrent_action;
    }

    FG_ACTOR_GET_P_RESULT(fg_lp8_measurement_t, p_measurement_result, p_completed_action);
    NRF_LOG_INFO("Measurement: filtered %u (raw %u) PPM CO2, %d C.\n",
        p_measurement_result->conc_filtered_pc, p_measurement_result->conc_pc,
        p_measurement_result->temperature);

    fg_actor_action_t * p_next_action = FG_ACTOR_POST_MESSAGE(rtc, FG_RTC_START_TIMER);
    FG_ACTOR_SET_ARGS(p_next_action, m_main_idle_time);

    publish_measurement(
        p_next_transaction, p_measurement_result->conc_filtered_pc, FG_MQTT_TOPIC_CO2);

    FG_ACTOR_SET_TRANSACTION_RESULT_HANDLER(init_or_wakeup_result_handler);
}