#!/bin/sh
mkdir ../../backup
kubectl exec hetida4office-timescaledb -- tar czf - /var/lib/postgresql/data/ > ../../backup/timescaledb.tar.gz
kubectl exec hetida4office-grafana -- tar czf - /var/lib/grafana > ../../backup/grafana.tar.gz
kubectl exec hetida4office-zookeeper -- tar czf - /data /datalog > ../../backup/zookeeper.tar.gz
