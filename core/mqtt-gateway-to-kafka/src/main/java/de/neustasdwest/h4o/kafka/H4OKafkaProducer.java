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
class H4OKafkaProducer {
    private final KafkaTemplate<String, byte[]> kafkaTemplate;
    @Value("${kafka.topic.channel.value.name}")
    private String topicTimeSeries;

    public H4OKafkaProducer(final KafkaTemplate<String, byte[]> kafkaTemplate) {
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
