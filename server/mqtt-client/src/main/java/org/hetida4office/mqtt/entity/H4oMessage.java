package org.hetida4office.mqtt.entity;

import lombok.EqualsAndHashCode;
import lombok.NonNull;
import lombok.Value;

import java.time.Instant;

@Value
@EqualsAndHashCode(callSuper = true)
public class H4oMessage extends H4oEvent {
    H4oMessage(final @NonNull String channelId,
               final @NonNull Instant timestamp,
               final @NonNull String message) {
        super(channelId, timestamp);
        this.message = message;
    }

    String message;
}
