package de.neustasdwest.h4o.kafka;

import de.neustasdwest.h4o.common.model.Measurement;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Qualifier;
import org.springframework.stereotype.Component;

import java.util.List;


@Component
@Qualifier("timeScaleDbKafkaGateway")
public class TimeScaleDbKafkaGateway implements TimeScaleDbGateway {
    private final H4OKafkaProducer kafkaProducer;

    public TimeScaleDbKafkaGateway(final H4OKafkaProducer kafkaProducer) {
        this.kafkaProducer = kafkaProducer;
    }

    @Override
    public void sendTimeSeriesDecimals(final List<Measurement> measurement) {
        kafkaProducer.sendDecimalValues(measurement);
    }
}
