package org.hetida4office.mqtt.entity;

import lombok.EqualsAndHashCode;
import lombok.NonNull;
import lombok.Value;

import java.math.BigDecimal;
import java.time.Instant;

@Value
@EqualsAndHashCode(callSuper = true)
public class H4oMeasurement extends H4oEvent {
    H4oMeasurement(final @NonNull String channelId,
                   final @NonNull Instant timestamp,
                   final @NonNull BigDecimal measurement) {
        super(channelId, timestamp);
        this.measurement = measurement;
    }

    BigDecimal measurement;
}
