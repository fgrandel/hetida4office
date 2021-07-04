package org.hetida4office.mqtt.db;

import lombok.NonNull;
import org.hetida4office.mqtt.entity.H4oMeasurement;
import org.hetida4office.mqtt.entity.H4oMessage;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.stereotype.Repository;

import static java.sql.Timestamp.from;

@Repository
public class H4oRepository {
    private static final String INSERT_MEASUREMENT_QUERY =
            "INSERT INTO timeseries.measurements(channel_id, timestamp, measurement) VALUES(?, ?, ?)";
    private static final String INSERT_MESSAGE_QUERY =
            "INSERT INTO timeseries.messages(channel_id, timestamp, message) VALUES(?, ?, ?)";

    private final JdbcTemplate jdbcTemplate;

    public H4oRepository(@NonNull final JdbcTemplate jdbcTemplate) {
        this.jdbcTemplate = jdbcTemplate;
    }

    public void storeMeasurement(final H4oMeasurement measurement) {
        this.jdbcTemplate.update(
                INSERT_MEASUREMENT_QUERY,
                preparedStatement -> {
                    preparedStatement.setString(1, measurement.getChannelId());
                    preparedStatement.setTimestamp(2, from(measurement.getTimestamp()));
                    preparedStatement.setBigDecimal(3, measurement.getMeasurement());
                });
    }

    public void storeMessage(final H4oMessage message) {
        this.jdbcTemplate.update(
                INSERT_MESSAGE_QUERY,
                preparedStatement -> {
                    preparedStatement.setString(1, message.getChannelId());
                    preparedStatement.setTimestamp(2, from(message.getTimestamp()));
                    preparedStatement.setString(3, message.getMessage());
                });
    }
}
