package de.neustasdwest.h4o.connectormqtt.client.subscriber;

import de.neustasdwest.h4o.connectormqtt.client.H4OSingletonMqttClient;
import de.neustasdwest.h4o.connectormqtt.client.builder.TopicNamesBuilder;
import de.neustasdwest.h4o.kafka.TimeScaleDbGateway;
import lombok.extern.log4j.Log4j2;
import org.springframework.boot.context.event.ApplicationReadyEvent;
import org.springframework.context.event.EventListener;
import org.springframework.stereotype.Component;

import java.util.HashMap;
import java.util.Map;

@Log4j2
@Component
public class H4OMqttTopicSubsHelper {
    private final H4OSingletonMqttClient h4OSingletonMqttClient;
    private final TopicNamesBuilder topicNamesBuilder;
    private final TimeScaleDbGateway timeScaleDbGateway;
    private static final Map<String, H4OMqttTopicSubscriber> h4oMqttTopicSubscribers = new HashMap<>();

    public H4OMqttTopicSubsHelper(final H4OSingletonMqttClient h4OSingletonMqttClient,
                                 final TopicNamesBuilder topicNamesBuilder,
                                  final TimeScaleDbGateway timeScaleDbGateway) {

        this.h4OSingletonMqttClient = h4OSingletonMqttClient;
        this.timeScaleDbGateway = timeScaleDbGateway;
        this.topicNamesBuilder = topicNamesBuilder;
    }

    @EventListener(ApplicationReadyEvent.class)
    public void startListening() {
        h4OSingletonMqttClient.connect();
        topicNamesBuilder.buildTopicNames().forEach(topicName -> {
            final H4OMqttTopicSubscriber topicSubscriber = new H4OMqttTopicSubscriber(topicName);
            h4oMqttTopicSubscribers.put(topicName, topicSubscriber);
            log.info("Added new topic subscriber with name -> {}",topicName);
            h4oMqttTopicSubscribers.get(topicName).subscribeToTopic(timeScaleDbGateway);
            log.info("Started Listening to topic -> {}",topicName);
        });
    }
}
