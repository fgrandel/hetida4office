package org.hetida4office.mqtt.client;

import lombok.NonNull;
import lombok.extern.slf4j.Slf4j;
import org.eclipse.paho.client.mqttv3.*;
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence;
import org.hetida4office.mqtt.db.H4oRepository;
import org.hetida4office.mqtt.entity.H4oEvent;
import org.hetida4office.mqtt.entity.H4oMeasurement;
import org.hetida4office.mqtt.entity.H4oMeasurementType;
import org.hetida4office.mqtt.entity.H4oMessage;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.boot.context.event.ApplicationReadyEvent;
import org.springframework.context.event.EventListener;
import org.springframework.stereotype.Component;

import javax.net.ssl.*;
import java.io.FileInputStream;
import java.security.KeyStore;
import java.util.Set;

import static java.lang.String.format;

@Slf4j
@Component
public class H4oMqttClient {
    private final MqttClient mqttClient;
    private final String mqttBrokerUri;
    private final H4oMqttTopicsBuilder mqttTopicsBuilder;
    private final H4oRepository repository;
    private final SSLContext sslContext;

    public H4oMqttClient(@Value("${h4o.mqtt.broker.host}") @NonNull final String mqttBrokerHost,
                         @Value("${h4o.mqtt.broker.port}") final int mqttBrokerPort,
                         @Value("${h4o.mqtt.broker.key-store}") final String mqttBrokerKeyStore,
                         @Value("${h4o.mqtt.broker.key-store-pw}") final String mqttBrokerKeyStorePw,
                         @Value("${h4o.mqtt.broker.trust-store}") final String mqttBrokerTrustStore,
                         @Value("${h4o.mqtt.broker.trust-store-pw}") final String mqttBrokerTrustStorePw,
                         @NonNull final H4oMqttTopicsBuilder mqttTopicsBuilder,
                         @NonNull final H4oRepository repository) throws Exception {
        // Authenticate the client to the broker.
        final KeyStore keyStore = KeyStore.getInstance("pkcs12");
        keyStore.load(new FileInputStream(mqttBrokerKeyStore), mqttBrokerKeyStorePw.toCharArray());
        final KeyManagerFactory keyManagerFactory = KeyManagerFactory.getInstance(KeyManagerFactory.getDefaultAlgorithm());
        keyManagerFactory.init(keyStore, mqttBrokerKeyStorePw.toCharArray());
        final KeyManager[] keyManagers = keyManagerFactory.getKeyManagers();

        // Authenticate the broker to the client.
        final KeyStore trustStore = KeyStore.getInstance("jks");
        trustStore.load(new FileInputStream(mqttBrokerTrustStore), mqttBrokerTrustStorePw.toCharArray());
        final TrustManagerFactory trustManagerFactory = TrustManagerFactory.getInstance(TrustManagerFactory.getDefaultAlgorithm());
        trustManagerFactory.init(trustStore);
        final TrustManager[] trustManagers = trustManagerFactory.getTrustManagers();

        final SSLContext sslContext = SSLContext.getInstance("TLSv1.3");
        sslContext.init(keyManagers, trustManagers, null);
        this.sslContext = sslContext;

        final String clientId = "hetida4office-consumer";
        this.mqttBrokerUri = format("ssl://%s:%d", mqttBrokerHost, mqttBrokerPort);
        final MemoryPersistence persistence = new MemoryPersistence();
        this.mqttClient = new MqttClient(this.mqttBrokerUri, clientId, persistence);
        this.mqttTopicsBuilder = mqttTopicsBuilder;
        this.repository = repository;
    }

    private void connect() throws Exception {
        if (this.mqttClient.isConnected()) {
            log.info("Client was already connected...");
            return;
        }
        log.info("Connecting to mqtt client at {}...", this.mqttBrokerUri);
        final MqttConnectOptions connOpts = new MqttConnectOptions();
        connOpts.setAutomaticReconnect(true);
        connOpts.setSocketFactory(this.sslContext.getSocketFactory());
        this.mqttClient.connect(connOpts);
    }

    @EventListener(ApplicationReadyEvent.class)
    public void startListening() throws Exception {
        this.connect();
        this.mqttClient.setCallback(new H4oMqttCallback());
        final Set<String> topics = this.mqttTopicsBuilder.getTopics();
        this.mqttClient.subscribe(topics.toArray(new String[0]));
    }

    private class H4oMqttCallback implements MqttCallback {
        @Override
        public void connectionLost(final Throwable cause) {
            log.error("MQTT connection lost. Trying to reconnect...", cause);
        }

        @Override
        public void messageArrived(final String mqttTopic, final MqttMessage mqttMessage) {
            if (mqttMessage.isDuplicate()) {
                return;
            }
            final H4oEvent h4oEvent = H4oMeasurementType.convert(mqttTopic, mqttMessage);
            if (h4oEvent instanceof H4oMeasurement) {
                final H4oMeasurement h4oMeasurement = (H4oMeasurement) h4oEvent;
                H4oMqttClient.this.repository.storeMeasurement(h4oMeasurement);
                log.debug("Added Measurement: channelId {}, timestamp {}, measurement {}",
                        h4oMeasurement.getChannelId(), h4oMeasurement.getTimestamp(), h4oMeasurement.getMeasurement());
            } else {
                final H4oMessage h4oMessage = (H4oMessage) h4oEvent;
                H4oMqttClient.this.repository.storeMessage(h4oMessage);
                log.debug("Added Message: channelId {}, timestamp {}, message {}",
                        h4oMessage.getChannelId(), h4oMessage.getTimestamp(), h4oMessage.getMessage());
            }
        }

        @Override
        public void deliveryComplete(final IMqttDeliveryToken token) {
            throw new IllegalStateException("Never sent a message.");
        }
    }
}
