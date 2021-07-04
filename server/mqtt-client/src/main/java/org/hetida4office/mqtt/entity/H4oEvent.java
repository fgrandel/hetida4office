package org.hetida4office.mqtt.entity;

import lombok.Value;
import lombok.experimental.NonFinal;

import java.time.Instant;

@Value
@NonFinal
abstract public class H4oEvent {
    String channelId;
    Instant timestamp;
}
