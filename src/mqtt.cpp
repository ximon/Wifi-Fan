#include "Mqtt.h"
#include "PubSubClient.h"

#define DEBUG_MQTT

void Mqtt::setup(void (&onMqttEvent)(char *topic, byte *payload, unsigned int length))
{
    client.setClient(espClient);
    client.setCallback(onMqttEvent);
}

void Mqtt::setServer(char *ipAddress, int port)
{
    mqttServer = ipAddress;
    mqttPort = port;
    client.setServer(mqttServer, mqttPort);
}

void Mqtt::setCredentials(char *user, char *pass)
{
    mqttUser = user;
    mqttPass = pass;
    hasCredentials = true;
}

void Mqtt::loop()
{
    if (mqttPort == 0)
        return;

    if (!client.connected())
    {
        reconnect();
        return;
    }

    client.loop();
}

void Mqtt::callback(char *topic, byte *payload, unsigned int length)
{
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';

#ifdef DEBUG_MQTT
    Serial.print("MQTT->");
    Serial.print(topic);
    Serial.print("->");
    Serial.println(message);
#endif

    if (strcmp(topic, "fan/cmnd/oscillate") == 0)
    {
#ifdef DEBUG_MQTT
        Serial.println("MQTT->Handling state message");
#endif
        handleStateMessages(topic, message, length);
        return;
    }

    if (strcmp(topic, "fan/cmnd/speed") == 0 ||
        strcmp(topic, "fan/cmnd/timer") == 0)
    {
#ifdef DEBUG_MQTT
        Serial.println("MQTT->Handing value message");
#endif
        handleValueMessages(topic, message, length);
        return;
    }
}

void Mqtt::handleStateMessages(char *topic, char *message, unsigned int length)
{
    const size_t capacity = JSON_OBJECT_SIZE(1) + 20;
    DynamicJsonDocument doc(capacity);
    deserializeJson(doc, message);

    bool enable = false;

    if (doc["state"].is<bool>())
        enable = doc["state"];
    else if (doc["state"].is<const char *>())
        enable = strncmp(doc["state"], "true", 4) == 0;
    else
        return;

    if (strcmp(topic, "fan/cmnd/oscillate") == 0)
    {

        return;
    }
}

void Mqtt::handleValueMessages(char *topic, char *message, unsigned int length)
{
    int value;

    if (isDigit(message[0]))
    {
        value = atoi(message);
    }
    else
    {
        const size_t capacity = JSON_OBJECT_SIZE(1) + 20;
        DynamicJsonDocument doc(capacity);
        deserializeJson(doc, message);
        value = doc["value"];
    }

    if (strcmp(topic, "fan/cmnd/speed") == 0)
    {
        Serial.print("MQTT->Setting Fan Speed to ");
        Serial.println();
        return;
    }

    if (strcmp(topic, "fan/cmnd/timer") == 0)
    {
        Serial.print("MQTT->Setting Timer to ");
        Serial.println();
        return;
    }
}

void Mqtt::sendStatus()
{
    if (!client.connected())
        return;

#ifdef DEBUG_MQTT
    Serial.println("MQTT->Sending status json...");
#endif

    publish("hottub/state/status", "");
}

void Mqtt::publish(const char *topic, char *payload)
{
    client.publish_P(topic, (const uint8_t *)payload, strnlen(payload, 512), false);
}

void Mqtt::publish(const char *topic, int payload)
{
    char buffer[2];
    itoa(payload, buffer, 10);
    client.publish_P(topic, (const uint8_t *)buffer, strnlen(buffer, 512), false);
}

void Mqtt::subscribe()
{
    client.subscribe("hottub/cmnd/pump");
}

bool Mqtt::connect()
{
    char clientId[20];
    sprintf(clientId, "WiFiFan-%li", random(0xffff));

    if (hasCredentials)
        return client.connect(clientId, mqttUser, mqttPass);

    return client.connect(clientId);
}

const char *mqttStateToString(int state)
{
    switch (state)
    {
    case -4:
        return "MQTT_CONNECTION_TIMEOUT";
    case -3:
        return "MQTT_CONNECTION_LOST";
    case -2:
        return "MQTT_CONNECT_FAILED";
    case -1:
        return "MQTT_DISCONNECTED";
    case 0:
        return "MQTT_CONNECTED";
    case 1:
        return "MQTT_CONNECT_BAD_PROTOCOL";
    case 2:
        return "MQTT_CONNECT_BAD_CLIENT_ID";
    case 3:
        return "MQTT_CONNECT_UNAVAILABLE";
    case 4:
        return "MQTT_CONNECT_BAD_CREDENTIALS";
    case 5:
        return "MQTT_CONNECT_UNAUTHORIZED";
    default:
        return "UNKNOWN";
    }
}

unsigned long lastConnectAttempt = 0;
void Mqtt::reconnect()
{
    if (millis() < 5000 || millis() - 5000 < lastConnectAttempt)
        return;

    lastConnectAttempt = millis();

    if (!client.connected())
    {
        if (connect())
        {
#ifdef DEBUG_MQTT
            Serial.println("MQTT->Connected!");
#endif

            subscribe();
            sendConnected();
            sendStatus();
            return;
        }

#ifdef DEBUG_MQTT
        Serial.print("MQTT->Connection failed, state = ");
        Serial.print(mqttStateToString(client.state()));
        Serial.println(", try again in 5 seconds");
#endif
    }
}
