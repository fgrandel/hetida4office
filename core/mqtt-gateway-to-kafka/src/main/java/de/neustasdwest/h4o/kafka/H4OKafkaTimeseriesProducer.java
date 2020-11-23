package de.neustasdwest.h4o.kafka;

import com.fasterxml.jackson.core.JsonProcessingException;
import de.neustasdwest.h4o.common.MeasurementSerializer;
import de.neustasdwest.h4o.common.model.Measurement;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.kafka.core.KafkaProducerException;
import org.springframework.kafka.core.KafkaTemplate;
import org.springframework.retry.annotation.Backoff;
import org.springframework.retry.annotation.EnableRetry;
import org.springframework.retry.annotation.Retryable;
import org.springframework.stereotype.Component;

import java.util.List;

@EnableRetry
@Component
@Slf4j
<<<<<<< HEAD:core/mqtt-gateway-to-kafka/src/main/java/de/neustasdwest/h4o/kafka/H4OKafkaProducer.java
class H4OKafkaProducer {
=======
class H4OKafkaTimeseriesProducer {
>>>>>>> master:core/mqtt-gateway-to-kafka/src/main/java/de/neustasdwest/h4o/kafka/H4OKafkaTimeseriesProducer.java
    private final KafkaTemplate<String, byte[]> kafkaTemplate;
    @Value("${kafka.topic.channel.value.name}")
    private String topicTimeSeries;

<<<<<<< HEAD:core/mqtt-gateway-to-kafka/src/main/java/de/neustasdwest/h4o/kafka/H4OKafkaProducer.java
    public H4OKafkaProducer(final KafkaTemplate<String, byte[]> kafkaTemplate) {
=======
    public H4OKafkaTimeseriesProducer(final KafkaTemplate<String, byte[]> kafkaTemplate) {
>>>>>>> master:core/mqtt-gateway-to-kafka/src/main/java/de/neustasdwest/h4o/kafka/H4OKafkaTimeseriesProducer.java
        log.info("Initialize kafka producer for topic TS {} ", topicTimeSeries);
        this.kafkaTemplate = kafkaTemplate;
    }

    @Retryable(include = {KafkaProducerException.class}, maxAttemptsExpression = "#{${kafka.send.max.attempts}}",
            backoff = @Backoff(delayExpression = "#{${kafka.send.backoff.delay}}"))
    void sendDecimalValues(final List<Measurement> measurements) {
        measurements.forEach(measurement -> {
            try {
                kafkaTemplate.send(topicTimeSeries, MeasurementSerializer.serializeMeasurement(measurement));
            } catch (final JsonProcessingException e) {
                log.error("Error while sending measurement [{}] to kafka -> {}", measurement, e.getMessage());
            }
        });
    }
}
