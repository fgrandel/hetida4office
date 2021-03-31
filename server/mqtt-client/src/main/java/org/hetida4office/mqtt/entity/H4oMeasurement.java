package org.hetida4office.mqtt.entity;

import lombok.Value;

import java.math.BigDecimal;
import java.time.Instant;

@Value
public class H4oMeasurement {
    String channelId;
    Instant timestamp;
    BigDecimal measurement;
}
