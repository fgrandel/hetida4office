package de.neustasdwest.h4o.connectormqtt.client.subscriber;

import de.neustasdwest.h4o.connectormqtt.client.H4OSingletonMqttClient;
import de.neustasdwest.h4o.connectormqtt.client.builder.TopicNamesBuilder;
import de.neustasdwest.h4o.kafka.H4OKafkaTimeseriesTopic;
import lombok.extern.log4j.Log4j2;
import org.springframework.boot.context.event.ApplicationReadyEvent;
import org.springframework.context.event.EventListener;
import org.springframework.stereotype.Component;

import java.util.HashMap;
import java.util.Map;

@Log4j2
@Component
public class H4OMqttTopicsSubscriber {
    private final H4OSingletonMqttClient h4oSingletonMqttClient;
    private final TopicNamesBuilder topicNamesBuilder;
    private final H4OKafkaTimeseriesTopic h4oKafkaTimeseriesTopic;
    private static final Map<String, H4OMqttTopicToKafkaTimeseriesBridge> h4oMqttTopicToKafkaTimeseriesBridges = new HashMap<>();

    public H4OMqttTopicsSubscriber(final H4OSingletonMqttClient h4oSingletonMqttClient,
                                   final TopicNamesBuilder topicNamesBuilder,
                                   final H4OKafkaTimeseriesTopic h4oKafkaTimeseriesTopic) {

        this.h4oSingletonMqttClient = h4oSingletonMqttClient;
        this.h4oKafkaTimeseriesTopic = h4oKafkaTimeseriesTopic;
        this.topicNamesBuilder = topicNamesBuilder;
    }

    @EventListener(ApplicationReadyEvent.class)
    public void startListening() {
        h4oSingletonMqttClient.connect();
        topicNamesBuilder.buildTopicNames().forEach(topicName -> {
            final H4OMqttTopicToKafkaTimeseriesBridge h4oMqttTopicToKafkaTimeseriesBridge = new H4OMqttTopicToKafkaTimeseriesBridge(topicName);
            h4oMqttTopicToKafkaTimeseriesBridges.put(topicName, h4oMqttTopicToKafkaTimeseriesBridge);
            log.info("Added new topic subscriber with name -> {}",topicName);
            h4oMqttTopicToKafkaTimeseriesBridges.get(topicName).forwardMqttTo(h4oKafkaTimeseriesTopic);
            log.info("Started Listening to topic -> {}",topicName);
        });
    }
}
