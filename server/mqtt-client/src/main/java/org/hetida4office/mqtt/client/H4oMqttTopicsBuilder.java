package org.hetida4office.mqtt.client;

import lombok.Getter;
import lombok.NonNull;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Component;

import java.util.HashSet;
import java.util.Set;

import static java.lang.String.join;

@Slf4j
@Component
public class H4oMqttTopicsBuilder {
    @Getter
    final Set<String> topics = new HashSet<>();

    public H4oMqttTopicsBuilder(@Value("${h4o.mqtt.topics.namespace}") @NonNull final String topicPrefix,
                                @Value("${h4o.mqtt.topics.measurements}") @NonNull final String[] topicSuffixes,
                                @Value("${h4o.mqtt.topics.device-names}") @NonNull final String[] deviceNames) {
        log.info("Generating topic names...");
        for (final String topicSuffix : topicSuffixes) {
            for (final String deviceName : deviceNames) {
                final String mqttTopic = join("/", topicPrefix, deviceName, topicSuffix);
                log.info("Registered MQTT topic '{}'.", mqttTopic);
                this.topics.add(mqttTopic);
            }
        }
    }
}
