package de.neustasdwest.h4o.mqtttestsensorsclient.publisher;

import org.springframework.scheduling.annotation.EnableScheduling;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Component;

import de.neustasdwest.h4o.mqtttestsensorsclient.builder.H4OTopicNamesBuilder;
import de.neustasdwest.h4o.mqtttestsensorsclient.client.H4OMqttPublisherClient;
import de.neustasdwest.h4o.mqtttestsensorsclient.client.H4OMqttPublisherClientImpl;
import lombok.extern.log4j.Log4j2;

@Log4j2
@Component
@EnableScheduling
public class H4OMqttPublisherClientRunner {
    private final H4OMqttPublisherClient h4oMqttPublisherClient;
    private final H4OTopicNamesBuilder h4oTopicNamesBuilder;
    private final Runnable publisherTask;

    public H4OMqttPublisherClientRunner(final H4OMqttPublisherClient h4oMqttPublisherClient,
            final H4OTopicNamesBuilder h4oTopicNamesBuilder) {
        this.h4oMqttPublisherClient = h4oMqttPublisherClient;
        this.h4oTopicNamesBuilder = h4oTopicNamesBuilder;
        this.publisherTask = new PublisherTask(this.h4oTopicNamesBuilder.buildTopicNames());
    }

    @Scheduled(initialDelayString = "${mqtt.publisher.scheduled.task.initial.delay.seconds}", 
    fixedDelayString = "${mqtt.publisher.scheduled.task.fixed.delay.seconds}")
    public void startPublishing() {
        if(!H4OMqttPublisherClientImpl.getInstance().getState().isConnected()) {
            this.h4oMqttPublisherClient.connect();
        }
        log.info("Publishing fake sensor values...");
        publisherTask.run();
    }
}
