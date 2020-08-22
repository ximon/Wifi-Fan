#ifndef MQTT_H
#define MQTT_H

#include <ArduinoJson.h>
#include "PubSubClient.h"
#include <ESP8266WiFi.h>

class Mqtt
{
public:
    Mqtt();

    void callback(char *topic, byte *payload, unsigned int length);

    void setup(void (&onMqttEvent)(char *topic, byte *payload, unsigned int length));
    void setServer(char *ipAddress, int port);
    void setCredentials(char *user, char *pass);
    void loop();

    void sendStatus();

private:
    WiFiClient espClient;
    PubSubClient client;

    void reconnect();
    void subscribe();
    bool connect();
    void sendConnected();
    void publish(const char *topic, char *payload);
    void publish(const char *topic, int payload);

    char *mqttServer;
    int mqttPort;

    bool hasCredentials;
    char *mqttUser;
    char *mqttPass;

    void handleStateMessages(char *topic, char *message, unsigned int length);
    void handleValueMessages(char *topic, char *message, unsigned int length);
    void handleRawCommand(char *message);

    unsigned int validateCommand(char *message);
};

#endif
