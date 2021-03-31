package org.hetida4office.mqtt.client;

import lombok.NonNull;
import lombok.extern.slf4j.Slf4j;
import org.eclipse.paho.client.mqttv3.*;
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence;
import org.hetida4office.mqtt.db.H4oRepository;
import org.hetida4office.mqtt.entity.H4oMeasurement;
import org.hetida4office.mqtt.entity.H4oMeasurementType;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.boot.context.event.ApplicationReadyEvent;
import org.springframework.context.event.EventListener;
import org.springframework.stereotype.Component;

import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSocketFactory;
import java.security.NoSuchAlgorithmException;
import java.util.Set;

import static java.lang.String.format;
import static java.rmi.server.RMISocketFactory.getSocketFactory;

@Slf4j
@Component
public class H4oMqttClient {
    private final MqttClient mqttClient;
    private final String mqttBrokerUri;
    private final H4oMqttTopicsBuilder mqttTopicsBuilder;
    private final H4oRepository repository;

    public H4oMqttClient(@Value("${h4o.mqtt.broker.host}") @NonNull final String mqttBrokerHost,
                         @Value("${h4o.mqtt.broker.port}") final int mqttBrokerPort,
                         @NonNull final H4oMqttTopicsBuilder mqttTopicsBuilder,
                         @NonNull final H4oRepository repository) throws MqttException {
        final String clientId = "hetida4office-consumer";
        this.mqttBrokerUri = format("ssl://%s:%d", mqttBrokerHost, mqttBrokerPort);
        final MemoryPersistence persistence = new MemoryPersistence();
        this.mqttClient = new MqttClient(this.mqttBrokerUri, clientId, persistence);
        this.mqttTopicsBuilder = mqttTopicsBuilder;
        this.repository = repository;
    }

    private void connect() throws MqttException, NoSuchAlgorithmException {
        if (this.mqttClient.isConnected()) {
            log.info("Client was already connected...");
            return;
        }
        log.info("Connecting to mqtt client at {}...", this.mqttBrokerUri);
        final MqttConnectOptions connOpts = new MqttConnectOptions();
        connOpts.setAutomaticReconnect(true);
        SSLSocketFactory socketFactory = SSLContext.getDefault().getSocketFactory();
        connOpts.setSocketFactory(socketFactory);
        this.mqttClient.connect(connOpts);
    }

    @EventListener(ApplicationReadyEvent.class)
    public void startListening() throws MqttException, NoSuchAlgorithmException {
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
        public void messageArrived(final String topic, final MqttMessage message) throws Exception {
            if (message.isDuplicate()) {
                return;
            }
            final H4oMeasurement measurement = H4oMeasurementType.convertAndScale(topic, message);
            H4oMqttClient.this.repository.storeMeasurement(measurement);
            log.debug("Added Measurement: channelId {}, timestamp {}, measurement {}",
                    measurement.getChannelId(), measurement.getTimestamp(), measurement.getMeasurement());
        }

        @Override
        public void deliveryComplete(final IMqttDeliveryToken token) {
            throw new IllegalStateException("Never sent a message.");
        }
    }
}
