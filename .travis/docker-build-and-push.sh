#!/bin/sh

echo "build and push h4o images to docker hub..."

echo "$DOCKER_HUB_PASSWORD" | docker login -u "$DOCKER_HUB_USER_NAME" --password-stdin

docker build -f ../Dockerfile.h4o.connector.mqtt -t h4o/connector-mqtt:latest .
docker build -f ../Dockerfile.h4o.kafka.consumer -t h4o/kafka-consumer:latest .

docker tag h4o/connector-mqtt:$COMMIT h4o/connector-mqtt:latest
docker tag h4o/kafka-consumer:$COMMIT h4o/kafka-consumer:latest

docker push h4o/connector-mqtt
docker push h4o/kafka-consumer