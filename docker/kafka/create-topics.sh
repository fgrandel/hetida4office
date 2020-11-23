#!/bin/sh

set -e

kafka-topics.sh --create --zookeeper hetida4office-zookeeper.hetida4office.svc.cluster.local:2181 --replication-factor 1 --partitions 1 --topic timeseries