package de.neustasdwest.h4o.common.serializer;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.datatype.jsr310.JavaTimeModule;
import de.neustasdwest.h4o.common.model.Measurement;

public class MeasurementSerializer {
    public static byte[] serializeMeasurement(final Measurement measurement) throws JsonProcessingException {
        final ObjectMapper objectMapper = new ObjectMapper();
        objectMapper.registerModule(new JavaTimeModule());
        return objectMapper.writeValueAsBytes(measurement);
    }
}
