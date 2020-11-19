#!/bin/sh

echo "build and push h4o images to docker hub..."

echo "$DOCKER_HUB_PASSWORD" | docker login -u "$DOCKER_HUB_USER_NAME" --password-stdin

IMAGE_H4O_CONNECT_MQTT=hetida4office/connector-mqtt
IMAGE_H4O_KAFKA_CONSUMER=hetida4office/kafka-consumer

docker build -f ./Dockerfile.h4o.connector.mqtt -t $IMAGE_H4O_CONNECT_MQTT:latest .
docker build -f ./Dockerfile.h4o.kafka.consumer -t $IMAGE_H4O_KAFKA_CONSUMER:latest .

docker push $IMAGE_H4O_CONNECT_MQTT:latest
docker push $IMAGE_H4O_KAFKA_CONSUMER:latest