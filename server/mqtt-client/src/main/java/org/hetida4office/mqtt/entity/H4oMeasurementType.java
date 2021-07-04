package org.hetida4office.mqtt.entity;

import lombok.AllArgsConstructor;
import lombok.NonNull;
import org.eclipse.paho.client.mqttv3.MqttMessage;

import java.math.BigDecimal;
import java.time.Instant;

import static java.lang.Long.parseLong;
import static java.util.Arrays.stream;

@AllArgsConstructor
public enum H4oMeasurementType {
    TEMPERATURE("temp", 2),
    HUMIDITY("hum", 3),
    PRESSURE("press", 4),
    BATTERY("bat", 6),
    DEFAULT("-", 0);

    private final String channelPrefix;
    private final int scale;

    public static H4oEvent convert(@NonNull final String mqttTopic, final MqttMessage mqttMessage) {
        String h4oChannel = mqttTopic;
        final Instant timestamp = Instant.now();
        final String payload = new String(mqttMessage.getPayload());

        final String[] mqttTopicParts = mqttTopic.split("/");
        assert mqttTopicParts.length >= 3 && mqttTopicParts.length <= 4;
        final String h4oChannelPostfix = mqttTopicParts[mqttTopicParts.length - 1];
        if (h4oChannelPostfix.equals("msg")) {
            if (mqttTopicParts.length == 4) {
                // This is a message concerning one of the measurement channels
                // So we use the measurement's channel name.
                h4oChannel = h4oChannel.replace("/msg", "");
            }
            return new H4oMessage(h4oChannel, timestamp, payload);
        } else {
            final H4oMeasurementType measurementType = stream(H4oMeasurementType.values())
                    .filter(measurementTypeCandidate -> measurementTypeCandidate.channelPrefix.equals(h4oChannelPostfix))
                    .findFirst().orElse(DEFAULT);
            final long unscaledMeasurement = parseLong(payload);
            final BigDecimal scaledMeasurement = new BigDecimal(unscaledMeasurement).movePointLeft(measurementType.scale);
            return new H4oMeasurement(h4oChannel, timestamp, scaledMeasurement);
        }
    }
}
