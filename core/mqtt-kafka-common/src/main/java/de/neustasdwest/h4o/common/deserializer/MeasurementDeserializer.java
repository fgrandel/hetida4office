package de.neustasdwest.h4o.common.deserializer;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.datatype.jsr310.JavaTimeModule;
import de.neustasdwest.h4o.common.model.Measurement;

import java.io.IOException;

public class MeasurementDeserializer {
    public static Measurement deserializeMeasurement(final byte[] measurementAsBytes) throws IOException {
        final ObjectMapper objectMapper = new ObjectMapper();
        objectMapper.registerModule(new JavaTimeModule());
        return objectMapper.readValue(measurementAsBytes, Measurement.class);
    }
}
