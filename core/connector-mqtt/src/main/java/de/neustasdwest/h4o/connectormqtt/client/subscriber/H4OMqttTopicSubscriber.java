package de.neustasdwest.h4o.connectormqtt.client.subscriber;

import com.hivemq.client.mqtt.datatypes.MqttQos;
import de.neustasdwest.h4o.common.model.Measurement;
import de.neustasdwest.h4o.connectormqtt.client.H4OSingletonMqttClient;
import de.neustasdwest.h4o.kafka.TimeScaleDbGateway;
import lombok.extern.log4j.Log4j2;

import java.io.UnsupportedEncodingException;
import java.math.BigDecimal;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.Collections;
import java.util.Objects;
import java.util.Random;

@Log4j2
public class H4OMqttTopicSubscriber {
    private final String topicName;

    public H4OMqttTopicSubscriber(final String topicName) {
        this.topicName = topicName;
    }

    public void subscribeToTopic(final TimeScaleDbGateway h4WGateway) {
        H4OSingletonMqttClient.getInstance().toAsync().subscribeWith()
                .topicFilter(topicName)
                .qos(MqttQos.EXACTLY_ONCE)
                .callback(mqtt5Publish -> {
                    try {
                        final String topic = mqtt5Publish.getTopic().toString();
                        final Measurement measurement = buildMeasurement(topic, mqtt5Publish.getPayloadAsBytes());
                        log.info("Send {} to kafka...",measurement);
                        h4WGateway.sendTimeSeriesDecimals(Collections.singletonList(measurement));
                    } catch (final UnsupportedEncodingException e) {
                        log.info(e.getStackTrace());
                    }
                })
                .send();
    }

    private Measurement buildMeasurement(final String topicName, final byte[] value) throws UnsupportedEncodingException {
        Random random = new Random();
        final String channelId = buildChannelId(topicName);
        final BigDecimal measurement = convertRawValue(value, channelId);
        return Measurement.builder()
                .channelId(channelId)
                .measurement(measurement)
                .timestamp(Instant.now().minus(random.nextBoolean() ? 1 : 2 ,ChronoUnit.MILLIS))
                .build();
    }

    private String buildChannelId(final String topicName) {
        Objects.requireNonNull(topicName, "Topic name must not be null!");
        final String[] split = topicName.split("/");
        final StringBuilder stringBuilder = new StringBuilder();
        for(int i = 2; i < split.length; i++) {
            stringBuilder.append(split[i].toUpperCase());
            if(i < split.length - 1) {
                stringBuilder.append("-");
            }
        }
        return stringBuilder.toString();
    }

    private BigDecimal convertRawValue(final byte[] value, final String channelId) throws UnsupportedEncodingException {
        Objects.requireNonNull(topicName, "Topic name must not be null!");
        Objects.requireNonNull(value, "RAW Measurement value must not be null!");
        final String converted = new String(value, "UTF-8");
        final Double measurement = Double.parseDouble(converted);
        BigDecimal h4oMeasurement = BigDecimal.valueOf(measurement);
        switch (channelId) {
            case "TEMP": h4oMeasurement = BigDecimal.valueOf(measurement / 100L);
            default: BigDecimal.valueOf(measurement);
        }
        log.info("Topic -> {} :: Measurement -> {}", topicName,h4oMeasurement);
        return h4oMeasurement;
    }
}
