package de.neustasdwest.h4o.kafka;

import de.neustasdwest.h4o.common.model.Measurement;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Qualifier;
import org.springframework.stereotype.Component;

import java.util.List;


@Component
@Qualifier("timeScaleDbKafkaGateway")
<<<<<<< HEAD:core/mqtt-gateway-to-kafka/src/main/java/de/neustasdwest/h4o/kafka/TimeScaleDbKafkaGateway.java
public class TimeScaleDbKafkaGateway implements TimeScaleDbGateway {
    private final H4OKafkaProducer kafkaProducer;

    public TimeScaleDbKafkaGateway(final H4OKafkaProducer kafkaProducer) {
        this.kafkaProducer = kafkaProducer;
=======
@Slf4j
public class H4OKafkaTimeseriesTopicImpl implements H4OKafkaTimeseriesTopic {
    private final H4OKafkaTimeseriesProducer h4oKafkaTimeseriesProducer;

    public H4OKafkaTimeseriesTopicImpl(final H4OKafkaTimeseriesProducer h4oKafkaTimeseriesProducer) {
        this.h4oKafkaTimeseriesProducer = h4oKafkaTimeseriesProducer;
>>>>>>> master:core/mqtt-gateway-to-kafka/src/main/java/de/neustasdwest/h4o/kafka/H4OKafkaTimeseriesTopicImpl.java
    }

    @Override
    public void sendTimeSeriesDecimals(final List<Measurement> measurement) {
        h4oKafkaTimeseriesProducer.sendDecimalValues(measurement);
    }
}
