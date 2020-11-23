#!/bin/bash -x

IMAGES=(hetida4office/connector-mqtt hetida4office/kafka-consumer)
DOCKERFILES=(Dockerfile.h4o.connector.mqtt Dockerfile.h4o.kafka.consumer)

echo "build and push h4o images to docker hub..."

echo "$DOCKER_HUB_PASSWORD" | docker login -u "$DOCKER_HUB_USER_NAME" --password-stdin

VERSION=`cat ./VERSION`

for image in "${!IMAGES[@]}"; do
	IMAGE_TAG="${IMAGES[$image]}:$VERSION"
	docker build -f "${DOCKERFILES[$image]}" -t "$IMAGE_TAG" .
	docker push "$IMAGE_TAG"
done;
