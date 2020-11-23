resource "kubernetes_pod" "grafana" {
  metadata {
    name = "hetida4office-grafana"
    namespace = kubernetes_namespace.hetida4office.metadata[0].name
    labels = {
      application = "hetida4office"
      service = "grafana"
    }
  }
  spec {
    container {
      image = "grafana/grafana"
      name  = "grafana"
      port {
        container_port = 3000
      }
      volume_mount {
        name = "grafana-data"
        mount_path = "/var/lib/grafana"
        sub_path = "grafana"
      }
    }
    volume {
      name = "grafana-data"
      empty_dir {
        size_limit = "50M"
      }
    }
  }
}

resource "kubernetes_service" "grafana" {
  metadata {
    name = "hetida4office-grafana"
    namespace = kubernetes_namespace.hetida4office.metadata[0].name
    labels = {
        application = "hetida4office"
        service = "grafana"
    }
  }
  spec {
    selector = {
      service = "grafana"
    }
    port {
      port        = 80
      target_port = 3000
    }
    type = "LoadBalancer"
  }
}
