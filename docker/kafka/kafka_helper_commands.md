### Describe Group

Topic consumption
`kafka-consumer-groups.sh --bootstrap-server localhost:9092 --describe --group h4o_timeseries_consumer_group`

### CONSUME TOPIC FROM SMALLEST

`kafka-console-consumer.sh --bootstrap-server localhost:9092 --topic timeseries`

### CREATE TOPIC

`kafka-topics.sh --create --zookeeper zookeeper:2181 --replication-factor 1 --partitions 1 --topic timeseries`
