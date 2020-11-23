resource "kubernetes_pod" "zookeeper" {
  metadata {
    name = "hetida4office-zookeeper"
    namespace = kubernetes_namespace.hetida4office.metadata[0].name
    labels = {
      application = "hetida4office"
      service = "zookeeper"
    }
  }
  spec {
    container {
      image = "wurstmeister/zookeeper"
      name  = "hetida4office-zookeeper"
      port {
        container_port = 2181
      }
      env {
        name = "ZOO_MY_ID"
        value = 1
      }
      volume_mount {
        name = "zookeeper-data"
        mount_path = "/data"
        sub_path = "zookeeper/data"
      }
      volume_mount {
        name = "zookeeper-data"
        mount_path = "/datalog"
        sub_path = "zookeeper/datalog"
      }
    }
    volume {
      name = "zookeeper-data"
      azure_disk {
          caching_mode = "None"
          kind = "Managed"
          disk_name = "hetida4office"
          data_disk_uri = azurerm_managed_disk.h4o.id
      }
    }
  }
}

resource "kubernetes_pod" "kafka" {
  metadata {
    name = "hetida4office-kafka"
    namespace = kubernetes_namespace.hetida4office.metadata[0].name
    labels = {
      application = "hetida4office"
      service = "kafka"
    }
  }
  spec {
    container {
      image = "wurstmeister/kafka"
      name  = "hetida4office-kafka"
      port {
        container_port = 9092
      }
      env {
        name = "KAFKA_ZOOKEEPER_CONNECT"
        value = "hetida4office-zookeeper.hetida4office.svc.cluster.local:2181"
      }
      env {
        name = "KAFKA_ADVERTISED_LISTENERS"
        value = "PLAINTEXT://hetida4office-kafka.hetida4office.svc.cluster.local:9092,PLAINTEXT_LOCAL://localhost:9093"
      }
      env {
        name = "KAFKA_LISTENERS"
        value = "PLAINTEXT://0.0.0.0:9092,PLAINTEXT_LOCAL://localhost:9093"
      }
      env {
        name = "KAFKA_LISTENER_SECURITY_PROTOCOL_MAP"
        value = "PLAINTEXT:PLAINTEXT,PLAINTEXT_LOCAL:PLAINTEXT"
      }
      env {
        name = "KAFKA_INTER_BROKER_LISTENER_NAME"
        value = "PLAINTEXT"
      }
    }
  }
}

resource "kubernetes_service" "zookeeper" {
  metadata {
    name = "hetida4office-zookeeper"
    namespace = kubernetes_namespace.hetida4office.metadata[0].name
    labels = {
        application = "hetida4office"
        service = "zookeeper"
    }
  }
  spec {
    selector = {
      service = "zookeeper"
    }
    port {
      port        = 2181
      target_port = 2181
    }
    type = "ClusterIP"
  }
}

resource "kubernetes_service" "kafka" {
  metadata {
    name = "hetida4office-kafka"
    namespace = kubernetes_namespace.hetida4office.metadata[0].name
    labels = {
        application = "hetida4office"
        service = "kafka"
    }
  }
  spec {
    selector = {
      service = "kafka"
    }
    port {
      port        = 9092
      target_port = 9092
    }
    type = "ClusterIP"
  }
}