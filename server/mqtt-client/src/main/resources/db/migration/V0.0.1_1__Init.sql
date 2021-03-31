CREATE SCHEMA metadata;
CREATE SCHEMA timeseries;

CREATE TABLE metadata.thing_nodes
(
    id          VARCHAR(127) PRIMARY KEY,
    parent_id   VARCHAR(127) REFERENCES metadata.thing_nodes (id),
    name        VARCHAR(127) NOT NULL,
    description VARCHAR(255)
);

CREATE TABLE metadata.things
(
    id          VARCHAR(127) PRIMARY KEY,
    name        VARCHAR(127) NOT NULL,
    description VARCHAR(255)
);

CREATE TABLE metadata.thing_nodes_to_things
(
    thing_node_id VARCHAR(127) NOT NULL REFERENCES metadata.thing_nodes (id),
    thing_id      VARCHAR(127) NOT NULL REFERENCES metadata.things (id),
    PRIMARY KEY (thing_node_id, thing_id)
);

CREATE TABLE metadata.channels
(
    id          VARCHAR(127) PRIMARY KEY,
    thing_id    VARCHAR(127) REFERENCES metadata.things (id),
    name        VARCHAR(127) NOT NULL,
    description VARCHAR(255),
    unit        VARCHAR(10),
    is_writable BOOLEAN
);

CREATE TABLE timeseries.messages
(
    channel_id VARCHAR(127) NOT NULL references metadata.channels (id),
    timestamp  TIMESTAMP WITH TIME ZONE,
    message    VARCHAR(255) NOT NULL,
    PRIMARY KEY (timestamp, channel_id)
);

CREATE TABLE timeseries.measurements
(
    channel_id  VARCHAR(127)   NOT NULL references metadata.Channels (id),
    timestamp   TIMESTAMP WITH TIME ZONE,
    measurement NUMERIC(12, 6) NOT NULL,
    PRIMARY KEY (timestamp, channel_id)
);

-- Tune timescaledb for timeseries
SELECT create_hypertable('timeseries.measurements', 'timestamp');
ALTER TABLE timeseries.measurements
    SET (timescaledb.compress, timescaledb.compress_segmentby = 'channel_id');
SELECT add_compression_policy('timeseries.measurements', INTERVAL '7 days');

SELECT create_hypertable('timeseries.messages', 'timestamp');
ALTER TABLE timeseries.messages
    SET (timescaledb.compress, timescaledb.compress_segmentby = 'channel_id');
SELECT add_compression_policy('timeseries.messages', INTERVAL '7 days');

-- Insert devices structure

INSERT INTO metadata.thing_nodes (id, parent_id, name, description)
VALUES ('h4o', null, 'H4O Devices', 'Global Device Hierarchy');

INSERT INTO metadata.things(id, name, description)
VALUES ('h4o/s1', 'Sensor 1', 'CO2 Sensor im Wohnzimmer'),
       ('h4o/s2', 'Sensor 2', 'CO2 Sensor im Schlafzimmer');

INSERT INTO metadata.thing_nodes_to_things(thing_node_id, thing_id)
VALUES ('h4o', 'h4o/s1'),
       ('h4o', 'h4o/s2');

INSERT INTO metadata.channels(id, thing_id, name, description, unit, is_writable)
VALUES ('h4o/s1/co2', 'h4o/s1', 'H4O-S1: CO2', 'H4O Sensor 1: CO2', 'ppm', true),
       ('h4o/s1/temp', 'h4o/s1', 'H4O-S1: Temperature', 'H4O Sensor 1: Temperature', '°C', true),
       ('h4o/s1/press', 'h4o/s1', 'H4O-S1: Pressure', 'H4O Sensor 1: Pressure', 'hPa', true),
       ('h4o/s1/hum', 'h4o/s1', 'H4O-S1: Humidity', 'H4O Sensor 1: Humidity (rel)', '%', true),
       ('h4o/s1/bat', 'h4o/s1', 'H4O-S1: Battery', 'H4O Sensor 1: Battery Voltage', 'V', true),
       ('h4o/s2/co2', 'h4o/s2', 'H4O-S2: CO2', 'H4O Sensor 2: CO2', 'ppm', true),
       ('h4o/s2/temp', 'h4o/s2', 'H4O-S2: Temperature', 'H4O Sensor 2: Temperature', '°C', true),
       ('h4o/s2/press', 'h4o/s2', 'H4O-S2: Pressure', 'H4O Sensor 2: Pressure', 'hPa', true),
       ('h4o/s2/hum', 'h4o/s2', 'H4O-S2: Humidity', 'H4O Sensor 2: Humidity (rel)', '%', true),
       ('h4o/s2/bat', 'h4o/s2', 'H4O-S2: Battery', 'H4O Sensor 2: Battery Voltage', 'V', true);
