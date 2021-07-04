-- Insert devices structure

INSERT INTO metadata.things(id, name, description)
VALUES ('h4o/s3', 'Sensor 3', 'CO2 Sensor (#3)'),
       ('h4o/s4', 'Sensor 4', 'CO2 Sensor (#4)'),
       ('h4o/s5', 'Sensor 5', 'Environment Sensor (#5)'),
       ('h4o/s6', 'Sensor 6', 'Environment Sensor (#6)'),
       ('h4o/s7', 'Sensor 7', 'Environment Sensor (#7)'),
       ('h4o/s8', 'Sensor 8', 'Environment Sensor (#8)');

INSERT INTO metadata.thing_nodes_to_things(thing_node_id, thing_id)
VALUES ('h4o', 'h4o/s3'),
       ('h4o', 'h4o/s4'),
       ('h4o', 'h4o/s5'),
       ('h4o', 'h4o/s6'),
       ('h4o', 'h4o/s7'),
       ('h4o', 'h4o/s8');

INSERT INTO metadata.channels(id, thing_id, name, description, unit, is_writable)
VALUES ('h4o/s3/co2', 'h4o/s3', 'H4O-S3: CO2', 'H4O Sensor 3: CO2', 'ppm', true),
       ('h4o/s3/temp', 'h4o/s3', 'H4O-S3: Temperature', 'H4O Sensor 3: Temperature', '°C', true),
       ('h4o/s3/press', 'h4o/s3', 'H4O-S3: Pressure', 'H4O Sensor 3: Pressure', 'hPa', true),
       ('h4o/s3/hum', 'h4o/s3', 'H4O-S3: Humidity', 'H4O Sensor 3: Humidity (rel)', '%', true),
       ('h4o/s3/bat', 'h4o/s3', 'H4O-S3: Battery', 'H4O Sensor 3: Battery Voltage', 'V', true),
       ('h4o/s3/lux', 'h4o/s3', 'H4O-S3: Light', 'H4O Sensor 3: Light Intensity', 'lux', true),
       ('h4o/s3/pir', 'h4o/s3', 'H4O-S3: PIR', 'H4O Sensor 3: Passive Infrared', 'yes/no', true),

       ('h4o/s4/co2', 'h4o/s4', 'H4O-S4: CO2', 'H4O Sensor 4: CO2', 'ppm', true),
       ('h4o/s4/temp', 'h4o/s4', 'H4O-S4: Temperature', 'H4O Sensor 4: Temperature', '°C', true),
       ('h4o/s4/press', 'h4o/s4', 'H4O-S4: Pressure', 'H4O Sensor 4: Pressure', 'hPa', true),
       ('h4o/s4/hum', 'h4o/s4', 'H4O-S4: Humidity', 'H4O Sensor 4: Humidity (rel)', '%', true),
       ('h4o/s4/bat', 'h4o/s4', 'H4O-S4: Battery', 'H4O Sensor 4: Battery Voltage', 'V', true),
       ('h4o/s4/lux', 'h4o/s4', 'H4O-S4: Light', 'H4O Sensor 4: Light Intensity', 'lux', true),
       ('h4o/s4/pir', 'h4o/s4', 'H4O-S4: PIR', 'H4O Sensor 4: Passive Infrared', 'yes/no', true),

       ('h4o/s5/temp', 'h4o/s5', 'H4O-S5: Temperature', 'H4O Sensor 5: Temperature', '°C', true),
       ('h4o/s5/press', 'h4o/s5', 'H4O-S5: Pressure', 'H4O Sensor 5: Pressure', 'hPa', true),
       ('h4o/s5/hum', 'h4o/s5', 'H4O-S5: Humidity', 'H4O Sensor 5: Humidity (rel)', '%', true),
       ('h4o/s5/bat', 'h4o/s5', 'H4O-S5: Battery', 'H4O Sensor 5: Battery Voltage', 'V', true),
       ('h4o/s5/lux', 'h4o/s5', 'H4O-S5: Light', 'H4O Sensor 5: Light Intensity', 'lux', true),
       ('h4o/s5/pir', 'h4o/s5', 'H4O-S5: PIR', 'H4O Sensor 5: Passive Infrared', 'yes/no', true),

       ('h4o/s6/temp', 'h4o/s6', 'H4O-S6: Temperature', 'H4O Sensor 6: Temperature', '°C', true),
       ('h4o/s6/press', 'h4o/s6', 'H4O-S6: Pressure', 'H4O Sensor 6: Pressure', 'hPa', true),
       ('h4o/s6/hum', 'h4o/s6', 'H4O-S6: Humidity', 'H4O Sensor 6: Humidity (rel)', '%', true),
       ('h4o/s6/bat', 'h4o/s6', 'H4O-S6: Battery', 'H4O Sensor 6: Battery Voltage', 'V', true),
       ('h4o/s6/lux', 'h4o/s6', 'H4O-S6: Light', 'H4O Sensor 6: Light Intensity', 'lux', true),
       ('h4o/s6/pir', 'h4o/s6', 'H4O-S6: PIR', 'H4O Sensor 6: Passive Infrared', 'yes/no', true),

       ('h4o/s7/temp', 'h4o/s7', 'H4O-S7: Temperature', 'H4O Sensor 7: Temperature', '°C', true),
       ('h4o/s7/press', 'h4o/s7', 'H4O-S7: Pressure', 'H4O Sensor 7: Pressure', 'hPa', true),
       ('h4o/s7/hum', 'h4o/s7', 'H4O-S7: Humidity', 'H4O Sensor 7: Humidity (rel)', '%', true),
       ('h4o/s7/bat', 'h4o/s7', 'H4O-S7: Battery', 'H4O Sensor 7: Battery Voltage', 'V', true),
       ('h4o/s7/lux', 'h4o/s7', 'H4O-S7: Light', 'H4O Sensor 7: Light Intensity', 'lux', true),
       ('h4o/s7/pir', 'h4o/s7', 'H4O-S7: PIR', 'H4O Sensor 7: Passive Infrared', 'yes/no', true),

       ('h4o/s8/temp', 'h4o/s8', 'H4O-S8: Temperature', 'H4O Sensor 8: Temperature', '°C', true),
       ('h4o/s8/press', 'h4o/s8', 'H4O-S8: Pressure', 'H4O Sensor 8: Pressure', 'hPa', true),
       ('h4o/s8/hum', 'h4o/s8', 'H4O-S8: Humidity', 'H4O Sensor 8: Humidity (rel)', '%', true),
       ('h4o/s8/bat', 'h4o/s8', 'H4O-S8: Battery', 'H4O Sensor 8: Battery Voltage', 'V', true),
       ('h4o/s8/lux', 'h4o/s8', 'H4O-S8: Light', 'H4O Sensor 8: Light Intensity', 'lux', true),
       ('h4o/s8/pir', 'h4o/s8', 'H4O-S8: PIR', 'H4O Sensor 8: Passive Infrared', 'yes/no', true),

       ('h4o/s1/msg', 'h4o/s1', 'H4O-S1: Messages', 'H4O Sensor 1: Messages', 'char(255)', true),
       ('h4o/s2/msg', 'h4o/s2', 'H4O-S2: Messages', 'H4O Sensor 2: Messages', 'char(255)', true),
       ('h4o/s3/msg', 'h4o/s3', 'H4O-S3: Messages', 'H4O Sensor 3: Messages', 'char(255)', true),
       ('h4o/s4/msg', 'h4o/s4', 'H4O-S4: Messages', 'H4O Sensor 4: Messages', 'char(255)', true),
       ('h4o/s5/msg', 'h4o/s5', 'H4O-S5: Messages', 'H4O Sensor 5: Messages', 'char(255)', true),
       ('h4o/s6/msg', 'h4o/s6', 'H4O-S6: Messages', 'H4O Sensor 6: Messages', 'char(255)', true),
       ('h4o/s7/msg', 'h4o/s7', 'H4O-S7: Messages', 'H4O Sensor 7: Messages', 'char(255)', true),
       ('h4o/s8/msg', 'h4o/s8', 'H4O-S8: Messages', 'H4O Sensor 8: Messages', 'char(255)', true);
