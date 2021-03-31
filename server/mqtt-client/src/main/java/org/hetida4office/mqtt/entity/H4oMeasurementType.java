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

    public static H4oMeasurement convertAndScale(@NonNull final String mqttTopic, final MqttMessage mqttMessage) {
        final String[] mqttTopicParts = mqttTopic.split("/");
        assert mqttTopicParts.length > 1;
        final String h4oChannelPrefix = mqttTopicParts[mqttTopicParts.length - 1];
        final H4oMeasurementType measurementType = stream(H4oMeasurementType.values())
                .filter(measurementTypeCandidate -> measurementTypeCandidate.channelPrefix.equals(h4oChannelPrefix))
                .findFirst().orElse(DEFAULT);
        final long unscaledMeasurement = parseLong(new String(mqttMessage.getPayload()));
        final BigDecimal scaledMeasurement = new BigDecimal(unscaledMeasurement).movePointLeft(measurementType.scale);
        return new H4oMeasurement(mqttTopic, Instant.now(), scaledMeasurement);
    }
}
