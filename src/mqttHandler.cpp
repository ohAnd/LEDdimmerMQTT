#include "mqttHandler.h"
#include <version.h>
#include <ArduinoJson.h>

MQTTHandler *MQTTHandler::instance = nullptr;

MQTTHandler::MQTTHandler(const char *broker, int port, const char *user, const char *password, bool useTLS)
    : mqtt_broker(broker), mqtt_port(port), mqtt_user(user), mqtt_password(password), useTLS(useTLS)
//   client(useTLS ? wifiClientSecure : wifiClient) {
{
    if (useTLS)
    {
        wifiClientSecure.setInsecure();
        client.setClient(wifiClientSecure);
    }
    else
        client.setClient(wifiClient);
    deviceGroupName = "LEDdimmerMQTT";
    mqttMainTopicPath = "";
    gw_ipAddress = "";
    instance = this;
}

void MQTTHandler::subscribedMessageArrived(char *topic, byte *payload, unsigned int length)
{
    String incommingMessage = "#"; // fix initial char to avoid empty string
    for (uint8_t i = 0; i < length; i++)
        incommingMessage += (char)payload[i];

    // Serial.println("MQTT: Message arrived [" + String(topic) + "] -> '" + incommingMessage + "'");
    if (instance != nullptr)
    {
        incommingMessage = incommingMessage.substring(1, length + 1); //'#' has to be ignored
        if (String(topic) == "homeassistant/light/" + instance->mqttMainTopicPath + "/led0/dimmer/set")
        // if (String(topic) == instance->mqttMainTopicPath + "/led0/targetPercent")
        {

            int gotTarget = (incommingMessage).toInt();
            uint8_t setTarget = 0;
            if (gotTarget >= 0 && gotTarget <= 100)
                setTarget = gotTarget;
            else if (gotTarget > 100)
                setTarget = 100;
            else if (gotTarget < 0)
                setTarget = 0;
            Serial.println("MQTT: cleaned incoming message: '" + incommingMessage + "' (len: " + String(length) + ") + got targetPercent: " + String(gotTarget) + " -> new setTarget: " + String(setTarget));
            instance->lastDimmerSet.setValue = setTarget;
            instance->lastDimmerSet.setValueUpdate = true;
        }
        else if (String(topic) == "homeassistant/light/" + instance->mqttMainTopicPath + "/led0/switch/set")
        // else if (String(topic) == instance->mqttMainTopicPath + "/led0/switch/set")
        {
            if(incommingMessage == "ON")
                instance->lastDimmerSet.setSwitch = true;
            else if(incommingMessage == "OFF")
                instance->lastDimmerSet.setSwitch = false;
            Serial.println("MQTT: cleaned incoming message: '" + incommingMessage + "' (len: " + String(length) + ") + got switchSet: " + incommingMessage + " -> new setTarget: " + String(instance->lastDimmerSet.setValue));
            instance->lastDimmerSet.setSwitchUpdate = true;
        }
    }
}

LedDimmerSet MQTTHandler::getLedDimmerSet()
{
    LedDimmerSet lastSetting = lastDimmerSet;
    lastDimmerSet.setValueUpdate = false;
    lastDimmerSet.setSwitchUpdate = false;
    return lastSetting;
}

void MQTTHandler::setup()
{
    Serial.println("MQTT:\t\t setup callback for subscribed messages");
    client.setCallback(subscribedMessageArrived);
    requestMQTTconnectionResetFlag = true;
    setupDone = true;
}

void MQTTHandler::loop()
{
    if (!client.connected() && setupDone)
    {
        reconnect();
    }
    else if (requestMQTTconnectionResetFlag)
    {
        initiateDiscoveryMessages(autoDiscoveryActiveRemove);
        Serial.println("MQTT:\t\t HA auto discovery messages " + String(autoDiscoveryActiveRemove ? "removed" : "send"));
        requestMQTTconnectionResetFlag = false; // reset request
        autoDiscoveryActiveRemove = false;      // reset remove
        // stop connection to force a reconnect with the new values for the whole connection
        stopConnection();
    }
    client.loop();
}

void MQTTHandler::publishDiscoveryMessage(const char *entity, const char *entityReadableName, const char *unit, bool deleteMessage, const char *icon, const char *deviceClass, const char *commandEntity)
{
    String uniqueID = String(deviceGroupName) + "_" + String(entity);
    String entityGroup = String(entity).substring(0, String(entity).indexOf("_"));
    String entityName = String(entity).substring(String(entity).indexOf("_") + 1);
    String commandEntityName = String(commandEntity).substring(String(commandEntity).indexOf("_") + 1);

    String configTopicPath = "homeassistant/light/" + String(deviceGroupName) + "/" + String(entity) + "/config";
    // Create the state topic path for the entity e.g. "LEDdimmerMQTT_12345678/grid/U"
    String stateTopicPath = "homeassistant/light/" + String(deviceGroupName) + "/" + String(entity) + "/state";
    if (String(deviceGroupName) != mqttMainTopicPath)
        stateTopicPath = String(mqttMainTopicPath) + "/" + entityGroup + "/" + entityName;
    // Create the command topic path for the entity e.g. "LEDdimmerMQTT_12345678/grid/U"
    String commandTopicPath = "homeassistant/light/" + String(deviceGroupName) + "/" + String(commandEntity) + "/set";
    if (String(deviceGroupName) != mqttMainTopicPath)
        commandTopicPath = String(mqttMainTopicPath) + "/" + entityGroup + "/" + commandEntityName;

    JsonDocument doc;
    doc["name"] = String(entityReadableName);

    if (deviceClass != NULL && String(deviceClass) == "brightness")
    {
        doc["state_topic"] = "homeassistant/light/" + String(mqttMainTopicPath) + "/" + entityGroup + "/switch/state";
        doc["command_topic"] = "homeassistant/light/" + String(mqttMainTopicPath) + "/" + entityGroup + "/switch/set";
        // doc["state_topic"] = stateTopicPath;
        // doc["command_topic"] = commandTopicPath;
        doc["brightness_state_topic"] = "homeassistant/light/" + String(mqttMainTopicPath) + "/" + entityGroup + "/" + String(commandEntity) + "/state";
        doc["brightness_command_topic"] = "homeassistant/light/" + String(mqttMainTopicPath) + "/" + entityGroup + "/" + String(commandEntity) + "/set";
        doc["brightness_scale"] = 100;
        // doc["brightness"] = true;
        // doc["supported_color_modes"] = "[\"brightness\"]";
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        // doc["on_command_type"] = "brightness";
        // doc["off_command_type"] = "brightness";
        doc["device_class"] = "light";
    }
    else if (deviceClass != NULL)
    {
        doc["state_topic"] = stateTopicPath;
        if (commandEntity != NULL)
        {
            doc["command_topic"] = commandTopicPath;
        }
        doc["device_class"] = deviceClass;
        if (String(deviceClass) == "timestamp")
            doc["value_template"] = "{{ as_datetime(value) }}";
    }

    if (unit != NULL)
        doc["unit_of_measurement"] = unit;

    if (icon != NULL)
        doc["icon"] = icon;

    doc["unique_id"] = uniqueID;
    doc["device"]["name"] = String(deviceGroupName);
    doc["device"]["identifiers"] = deviceGroupName;
    doc["device"]["manufacturer"] = "ohAnd";
    doc["device"]["model"] = "LEDdimmerMQTT ESP8266/ESP32";
    doc["device"]["hw_version"] = "1.0";
    doc["device"]["sw_version"] = String(VERSION);
    // doc["device"]["configuration_url"] = "http://" + String(deviceGroupName);
    doc["device"]["configuration_url"] = "http://" + gw_ipAddress;

    char payload[1024];
    size_t len = serializeJson(doc, payload);

    if (!deleteMessage)
    {
        client.beginPublish(configTopicPath.c_str(), len, true);
        client.print(payload);
        client.endPublish();
        // Serial.println("\nHA autoDiscovery - send JSON to broker at " + String(config_topic));
    }
    else
    {
        client.publish(configTopicPath.c_str(), NULL, false); // delete message without retain
    }
}

void MQTTHandler::publishStandardData(String entity, String value)
{
    entity.replace("_", "/");
    String stateTopicPath = "homeassistant/light/" + String(deviceGroupName) + "/" + String(entity) + "/state";
    // if (String(deviceGroupName) != mqttMainTopicPath || !autoDiscoveryActive)
    //     stateTopicPath = String(mqttMainTopicPath) + "/" + entity;

    client.publish(stateTopicPath.c_str(), value.c_str(), true);
}

boolean MQTTHandler::initiateDiscoveryMessages(bool autoDiscoveryRemove)
{
    if (client.connected())
    {
        if (autoDiscoveryActive || autoDiscoveryRemove)
        {
            if (!autoDiscoveryRemove)
                Serial.println("MQTT:\t\t setup HA auto discovery for all entities of this device");
            else
                Serial.println("MQTT:\t\t removing devices for HA auto discovery");

            // Publish MQTT auto-discovery messages

            publishDiscoveryMessage("led0", "dimming LED 0", NULL, autoDiscoveryRemove, NULL, "brightness", "dimmer");
            // publishDiscoveryMessage("led0_rawValueStep", "animation steps", " ", autoDiscoveryRemove, NULL, NULL);
            // publishDiscoveryMessage("led0_rawValueStepDelay", "animation delay in ms", "%", autoDiscoveryRemove, NULL, NULL);

            // publishDiscoveryMessage("timestamp", "Time stamp", NULL, autoDiscoveryRemove, NULL, "timestamp");
            return true;
        }
        else
        {
            Serial.println("MQTT:\t\t HA auto discovery is disabled, no publish of auto discovery messages");
            return false;
        }
    }
    else
    {
        Serial.println("MQTT:\t\t MQTT not connected, can't send HA auto discovery messages");
        return false;
    }
}

void MQTTHandler::reconnect()
{
    if (!client.connected() && (millis() - lastReconnectAttempt > 5000))
    {
        Serial.println("\nMQTT:\t\t Attempting connection... (HA AutoDiscover: " + String(autoDiscoveryActive) + ") ... ");
        if (client.connect(deviceGroupName, mqtt_user, mqtt_password))
        {
            Serial.println("\nMQTT:\t\t Attempting connection is now connected");

            client.subscribe(("homeassistant/light/" + instance->mqttMainTopicPath + "/led0/dimmer/set").c_str());
            Serial.println("MQTT:\t\t subscribe to: " + ("homeassistant/light/" + instance->mqttMainTopicPath + "/led0/dimmer/set"));
            client.subscribe(("homeassistant/light/" + instance->mqttMainTopicPath + "/led0/switch/set").c_str());
            Serial.println("MQTT:\t\t subscribe to: " + ("homeassistant/light/" + instance->mqttMainTopicPath + "/led0/switch/set"));
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.println(client.state());
            lastReconnectAttempt = millis();
        }
    }
}

void MQTTHandler::stopConnection(boolean full)
{
    if (client.connected())
    {
        client.disconnect();
        Serial.println("MQTT:\t\t ... stopped connection");
        // if(full) {
        //     delete &client;
        //     Serial.println("MQTT:\t\t ... with freeing memory");
        // }
    }
    else
    {
        Serial.println("MQTT:\t\t ... tried stop connection - no connection established");
    }
}

// Setter methods for runtime configuration
void MQTTHandler::setBroker(const char *broker)
{
    stopConnection();
    mqtt_broker = broker;
    client.setServer(mqtt_broker, mqtt_port);
}

void MQTTHandler::setPort(int port)
{
    stopConnection();
    mqtt_port = port;
    client.setServer(mqtt_broker, mqtt_port);
}

void MQTTHandler::setUser(const char *user)
{
    stopConnection();
    mqtt_user = user;
}

void MQTTHandler::setPassword(const char *password)
{
    stopConnection();
    mqtt_password = password;
}

void MQTTHandler::setUseTLS(bool useTLS)
{
    stopConnection();
    this->useTLS = useTLS;
    if (useTLS)
    {
        wifiClientSecure.setInsecure();
        client.setClient(wifiClientSecure);
        Serial.println("MQTT:\t\t setUseTLS: initialized with TLS");
    }
    else
    {
        client.setClient(wifiClient);
        Serial.println("MQTT:\t\t setUseTLS: initialized without TLS");
    }
    // client.setClient(useTLS ? wifiClientSecure : wifiClient);
    client.setServer(mqtt_broker, mqtt_port);
}

void MQTTHandler::setMainTopic(String mainTopicPath)
{
    stopConnection();
    mqttMainTopicPath = mainTopicPath;
}

// Setter method to combine all settings
void MQTTHandler::setConfiguration(const char *broker, int port, const char *user, const char *password, bool useTLS, const char *sensorUniqueName, const char *mainTopicPath, bool autoDiscovery, const char *ipAddress)
{
    mqtt_broker = broker;
    mqtt_port = port;
    mqtt_user = user;
    mqtt_password = password;
    this->useTLS = useTLS;
    setUseTLS(useTLS);
    client.setServer(mqtt_broker, mqtt_port);
    deviceGroupName = sensorUniqueName;
    mqttMainTopicPath = mainTopicPath;
    autoDiscoveryActive = autoDiscovery;
    gw_ipAddress = ipAddress;
    Serial.println("MQTT:\t\t config for broker: '" + String(mqtt_broker) + "' on port: '" + String(mqtt_port) + "'" + " and user: '" + String(mqtt_user) + "' with TLS: " + String(useTLS));
}

void MQTTHandler::requestMQTTconnectionReset(boolean autoDiscoveryRemoveRequested)
{
    requestMQTTconnectionResetFlag = true;
    autoDiscoveryActiveRemove = autoDiscoveryRemoveRequested;
    Serial.println("MQTT:\t\t request for MQTT connection reset - with HA auto discovery " + String(autoDiscoveryRemoveRequested ? "remove" : "send"));
}
