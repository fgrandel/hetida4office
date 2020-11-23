#ifndef FG_MQTTSN_ACTOR_H
#define FG_MQTTSN_ACTOR_H

#include "actor/fg_actor.h"

typedef enum
{
    FG_MQTTSN_CONNECT,
    FG_MQTTSN_DISCONNECT,
    FG_MQTTSN_PUBLISH,
} fg_mqttsn_actor_message_code_t;

typedef enum
{
    FG_MQTT_TOPIC_PRESSURE,
    FG_MQTT_TOPIC_TEMPERATURE,
    FG_MQTT_TOPIC_HUMIDITY,
    FG_MQTT_TOPIC_CO2,
    FG_MQTT_TOPIC_STATUS,
    FG_MQTT_TOPIC_NUM
} fg_mqttsn_topic_t;

typedef struct
{
    uint16_t topic_id;
    uint8_t * p_data;
    size_t size;
} fg_mqttsn_message_t;

FG_ACTOR_INTERFACE_DEC(mqttsn, fg_mqttsn_actor_message_code_t);

#endif // FG_MQTTSN_ACTOR_H__