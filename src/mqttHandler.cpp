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
    String incommingTopic = String(topic);
    String incommingMessage = "#"; // fix initial char to avoid empty string
    for (uint8_t i = 0; i < length; i++)
        incommingMessage += (char)payload[i];

    // Serial.println("MQTT: Message arrived [" + String(incommingTopic) + "] -> '" + incommingMessage + "'");
    if (instance != nullptr)
    {
        incommingMessage = incommingMessage.substring(1, length + 1); //'#' has to be ignored
        // filter message to check if it contains "homeassistant/light/" + instance->mqttMainTopicPath + "/led"
        if (incommingTopic.indexOf("homeassistant/light/" + instance->mqttMainTopicPath + "/led") >= 0 || incommingTopic.indexOf("homeassistant/number/" + instance->mqttMainTopicPath + "/led") >= 0)
        {
            // Serial.println("MQTT: recognized Message arrived [" + incommingTopic + "] -> '" + incommingMessage + "'");
            // get the led number for dimmer or for switch
            int ledNo = incommingTopic.substring(incommingTopic.indexOf("led") + 3, incommingTopic.indexOf("/", incommingTopic.indexOf("led") + 3)).toInt();
            // Serial.println("MQTT: cleaned incoming message: '" + incommingMessage + "' got ledNo: " + String(ledNo));

            // get the command for dimmer or for switch
            if (incommingTopic == "homeassistant/light/" + instance->mqttMainTopicPath + "/led" + String(ledNo) + "/dimmer/set")
            {
                int gotTarget = (incommingMessage).toInt();
                uint8_t setTarget = 0;
                if (gotTarget >= 0 && gotTarget <= 100)
                    setTarget = gotTarget;
                else if (gotTarget > 100)
                    setTarget = 100;
                else if (gotTarget < 0)
                    setTarget = 0;
                Serial.println("MQTT: led number: " + String(ledNo) + " got targetPercent: " + String(gotTarget) + " -> new setTarget: " + String(setTarget));
                instance->lastDimmerSet[ledNo].setValue = setTarget;
                instance->lastDimmerSet[ledNo].setValueUpdate = true;
            }
            else if (incommingTopic == "homeassistant/light/" + instance->mqttMainTopicPath + "/led" + String(ledNo) + "/switch/set")
            {
                if (incommingMessage == "ON")
                    instance->lastDimmerSet[ledNo].setSwitch = true;
                else if (incommingMessage == "OFF")
                    instance->lastDimmerSet[ledNo].setSwitch = false;
                Serial.println("MQTT: led number: " + String(ledNo) + " got switchSet: " + incommingMessage + " -> new setTarget: " + String(instance->lastDimmerSet[ledNo].setSwitch));
                instance->lastDimmerSet[ledNo].setSwitchUpdate = true;
            }
            else if (incommingTopic == "homeassistant/number/" + instance->mqttMainTopicPath + "/led" + String(ledNo) + "_dimValueStep/set")
            {
                int gotTarget = (incommingMessage).toInt();
                uint8_t setTarget = 0;
                if (gotTarget >= 0 && gotTarget <= 100)
                    setTarget = gotTarget;
                else if (gotTarget > 100)
                    setTarget = 100;
                else if (gotTarget < 0)
                    setTarget = 0;
                Serial.println("MQTT: led number: " + String(ledNo) + " got dimValueStep: " + String(gotTarget) + " -> new setTarget: " + String(setTarget));
                instance->lastDimmerSet[ledNo].setdimValueStep = setTarget;
                instance->lastDimmerSet[ledNo].setdimValueStepUpdate = true;
            }
            else if (incommingTopic == "homeassistant/number/" + instance->mqttMainTopicPath + "/led" + String(ledNo) + "_dimValueStepDelay/set")
            {
                int gotTarget = (incommingMessage).toInt();
                uint8_t setTarget = 0;
                if (gotTarget >= 0 && gotTarget <= 100)
                    setTarget = gotTarget;
                else if (gotTarget > 100)
                    setTarget = 100;
                else if (gotTarget < 0)
                    setTarget = 0;
                Serial.println("MQTT: led number: " + String(ledNo) + " got dimValueStepDelay: " + String(gotTarget) + " -> new setTarget: " + String(setTarget));
                instance->lastDimmerSet[ledNo].setdimValueStepDelay = setTarget;
                instance->lastDimmerSet[ledNo].setdimValueStepDelayUpdate = true;
            }
        }
        else
        {
            Serial.println("MQTT: NOT RECOGNIZED Message arrived [" + incommingTopic + "] -> '" + incommingMessage + "'");
        }
    }
}

LedDimmerSet MQTTHandler::getLedDimmerSet(uint8_t ledNo)
{
    LedDimmerSet lastSetting = lastDimmerSet[ledNo];
    lastDimmerSet[ledNo].setValueUpdate = false;
    lastDimmerSet[ledNo].setSwitchUpdate = false;
    lastDimmerSet[ledNo].setdimValueStepUpdate = false;
    lastDimmerSet[ledNo].setdimValueStepDelayUpdate = false;
    return lastSetting;
}

void MQTTHandler::setup(uint8_t ledPWMpin_0, uint8_t ledPWMpin_1, uint8_t ledPWMpin_2, uint8_t ledPWMpin_3, uint8_t ledPWMpin_4)
{
    lastDimmerSet[0].ledPWMpin = ledPWMpin_0;
    lastDimmerSet[1].ledPWMpin = ledPWMpin_1;
    lastDimmerSet[2].ledPWMpin = ledPWMpin_2;
    lastDimmerSet[3].ledPWMpin = ledPWMpin_3;
    lastDimmerSet[4].ledPWMpin = ledPWMpin_4;
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

    String categoryName = "sensor";
    String configTopicPath = "homeassistant/" + categoryName + "/" + String(deviceGroupName) + "/" + String(entity) + "/config";
    // Create the state topic path for the entity e.g. "LEDdimmerMQTT_12345678/grid/U"
    String stateTopicPath = "homeassistant/" + categoryName + "/" + String(deviceGroupName) + "/" + String(entity) + "/state";
    if (String(deviceGroupName) != mqttMainTopicPath)
        stateTopicPath = String(mqttMainTopicPath) + "/" + entityGroup + "/" + entityName;
    // Create the command topic path for the entity e.g. "LEDdimmerMQTT_12345678/grid/U"
    String commandTopicPath = "homeassistant/" + categoryName + "/" + String(deviceGroupName) + "/" + String(commandEntity) + "/set";
    if (String(deviceGroupName) != mqttMainTopicPath)
        commandTopicPath = String(mqttMainTopicPath) + "/" + entityGroup + "/" + commandEntityName;

    Serial.println("MQTT:\t\t HA auto discovery for entity: " + String(entity) + " - stateTopic: " + stateTopicPath + " - commandTopic: " + commandTopicPath);

    JsonDocument doc;
    doc["name"] = String(entityReadableName);

    if (deviceClass != NULL && String(deviceClass) == "brightness")
    {
        configTopicPath = "homeassistant/light/" + String(deviceGroupName) + "/" + String(entity) + "/config";
        doc["state_topic"] = "homeassistant/light/" + String(mqttMainTopicPath) + "/" + entityGroup + "/switch/state";
        doc["command_topic"] = "homeassistant/light/" + String(mqttMainTopicPath) + "/" + entityGroup + "/switch/set";
        doc["brightness_state_topic"] = "homeassistant/light/" + String(mqttMainTopicPath) + "/" + entityGroup + "/" + String(commandEntity) + "/state";
        doc["brightness_command_topic"] = "homeassistant/light/" + String(mqttMainTopicPath) + "/" + entityGroup + "/" + String(commandEntity) + "/set";
        doc["brightness_scale"] = 100;
        doc["payload_on"] = "ON";
        doc["payload_off"] = "OFF";
        doc["device_class"] = "light";
    }
    else if (deviceClass != NULL && String(deviceClass) == "number")
    {
        uniqueID = uniqueID + "_" + commandEntity;
        configTopicPath = "homeassistant/number/" + String(deviceGroupName) + "/" + entityGroup + "_" + String(commandEntity) + "/config";
        doc["state_topic"] = "homeassistant/number/" + String(mqttMainTopicPath) + "/" + entityGroup + "_" + String(commandEntity) + "/state";
        doc["command_topic"] = "homeassistant/number/" + String(mqttMainTopicPath) + "/" + entityGroup + "_" + String(commandEntity) + "/set";
        doc["mode"] = "box";
        doc["min"] = 1;
        if (String(commandEntity) == "dimValueStep")
            doc["max"] = 50;
        else
            doc["max"] = 99;
        doc["entity_category"] = "diagnostic";
    }
    else if (deviceClass != NULL)
    {
        doc["state_topic"] = stateTopicPath;
        if (commandEntity != NULL)
        {
            doc["command_topic"] = commandTopicPath;
        }
        doc["device_class"] = deviceClass;
        // if (String(deviceClass) == "timestamp")
            // doc["value_template"] = "{{ as_datetime(value) }}";
        doc["entity_category"] = "diagnostic";
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
    doc["device"]["hw_version"] = "1.0 (" + platformData.chipType + ")";
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
        client.publish(configTopicPath.c_str(), NULL, true); // delete message with retain
    }
}

void MQTTHandler::publishStandardData(String subDevice, String entity, String value, String topicClass)
{
    if (entity != "")
        subDevice = subDevice + "/" + entity;
    String stateTopicPath = "homeassistant/" + topicClass + "/" + String(deviceGroupName) + "/" + String(subDevice) + "/state";

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
            for (uint8_t i = 0; i < 5; i++)
            {
                if (lastDimmerSet[i].ledPWMpin != 255 && lastDimmerSet[i].ledPWMpin != 0)
                {
                    publishDiscoveryMessage(("led" + String(i)).c_str(), ("LED " + String(i) + " dimming").c_str(), NULL, autoDiscoveryRemove, NULL, "brightness", "dimmer");
                    publishDiscoveryMessage(("led" + String(i)).c_str(), ("LED " + String(i) + " step").c_str(), NULL, autoDiscoveryRemove, "mdi:step-forward", "number", "dimValueStep");
                    publishDiscoveryMessage(("led" + String(i)).c_str(), ("LED " + String(i) + " step delay (ms)").c_str(), NULL, autoDiscoveryRemove, "mdi:sort-clock-descending-outline", "number", "dimValueStepDelay");
                }
            }
            publishDiscoveryMessage("timestamp", "Timestamp", NULL, autoDiscoveryRemove, "mdi:clock-time-eight-outline","timestamp");
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

void MQTTHandler::subscribingLEDxLight(uint8_t ledNo)
{
    String topic = "homeassistant/light/" + instance->mqttMainTopicPath + "/led" + String(ledNo) + "/dimmer/set";
    client.subscribe(topic.c_str());
    Serial.print("MQTT:\t\t subscribe to: " + topic);
    topic = "homeassistant/light/" + instance->mqttMainTopicPath + "/led" + String(ledNo) + "/switch/set";
    client.subscribe(topic.c_str());
    Serial.println(" and: " + topic);
}

void MQTTHandler::subscribingLEDxNumber(uint8_t ledNo)
{
    String topic = "homeassistant/number/" + instance->mqttMainTopicPath + "/led" + String(ledNo) + "_dimValueStep/set";
    client.subscribe(topic.c_str());
    Serial.print("MQTT:\t\t subscribe to: " + topic);
    topic = "homeassistant/number/" + instance->mqttMainTopicPath + "/led" + String(ledNo) + "_dimValueStepDelay/set";
    client.subscribe(topic.c_str());
    Serial.println(" and: " + topic);
}

void MQTTHandler::reconnect()
{
    if (!client.connected() && (millis() - lastReconnectAttempt > 5000))
    {
        Serial.println("\nMQTT:\t\t Attempting connection... (HA AutoDiscover: " + String(autoDiscoveryActive) + ") ... ");
        if (client.connect(deviceGroupName, mqtt_user, mqtt_password))
        {
            Serial.println("\nMQTT:\t\t Attempting connection is now connected");

            for (uint8_t i = 0; i < 5; i++)
            {
                if (lastDimmerSet[i].ledPWMpin != 255 && lastDimmerSet[i].ledPWMpin != 0 )
                {
                    subscribingLEDxLight(i);
                    subscribingLEDxNumber(i);
                }
            }
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
