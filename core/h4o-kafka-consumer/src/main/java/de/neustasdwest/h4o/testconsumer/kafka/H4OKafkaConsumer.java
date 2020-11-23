package de.neustasdwest.h4o.testconsumer.kafka;

import static de.neustasdwest.h4o.common.MeasurementDeserializer.deserializeMeasurement;
import static java.util.Collections.singletonList;

import java.io.IOException;

import org.apache.kafka.clients.consumer.ConsumerRecord;
import org.springframework.kafka.annotation.KafkaListener;
import org.springframework.retry.annotation.Backoff;
import org.springframework.retry.annotation.Retryable;
import org.springframework.stereotype.Component;
import org.springframework.web.client.ResourceAccessException;

import de.neustasdwest.h4o.common.model.Measurement;
import de.neustasdwest.h4o.testconsumer.batch.TSWriterRepository;
import lombok.extern.slf4j.Slf4j;

@Slf4j
@Component
public class H4OKafkaConsumer {
    private final TSWriterRepository tsWriter;

    public H4OKafkaConsumer(final TSWriterRepository tsWriterRepository) {
        this.tsWriter = tsWriterRepository;
    }

    @KafkaListener(topics = "#{'${kafka.topic.channelvalue.name}'}")
    @Retryable(
            value = {ResourceAccessException.class},
            maxAttempts = 3,
            backoff = @Backoff(delay = 20000))
    public void listen(final ConsumerRecord<String, byte[]> cr) throws IOException, InterruptedException {
        log.info("Consuming Topic {} record from partition {} with offset {}.", cr.topic(), cr.partition(), cr.offset());
        final Measurement measurement = deserializeMeasurement(cr.value());
        this.tsWriter.batchInsertMeasurements(singletonList(measurement));
        log.info("Added Measurement: channelId::{}, timestamp::{}, measurement::{}",
                measurement.getChannelId(),
                measurement.getTimestamp(),
                measurement.getMeasurement());
    }
}
