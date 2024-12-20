#ifndef MQTTHANDLER_H
#define MQTTHANDLER_H

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

#include <base/platformData.h>

// MQTT_CONNECTION_TIMEOUT (-4): The server didn't respond within the keep-alive time.
// MQTT_CONNECTION_LOST (-3): The network connection was broken.
// MQTT_CONNECT_FAILED (-2): The network connection failed.
// MQTT_DISCONNECTED (-1): The client is disconnected.
// MQTT_CONNECTED (0): The client is connected.
// MQTT_CONNECT_BAD_PROTOCOL (1): The server doesn't support the requested version of MQTT.
// MQTT_CONNECT_BAD_CLIENT_ID (2): The server rejected the client identifier.
// MQTT_CONNECT_UNAVAILABLE (3): The server was unable to accept the connection.
// MQTT_CONNECT_BAD_CREDENTIALS (4): The username/password were rejected.
// MQTT_CONNECT_UNAUTHORIZED (5): The client was not authorized to connect.

struct LedDimmerSet {
    int8_t setValue = 0;
    boolean setValueUpdate = false;
    boolean setSwitch = false;
    boolean setSwitchUpdate = false;
    int8_t setdimValueStep = 0;
    boolean setdimValueStepUpdate = false;
    int8_t setdimValueStepDelay = 0;
    boolean setdimValueStepDelayUpdate = false;
    uint8_t ledPWMpin = 255;
};

class MQTTHandler {
public:
    MQTTHandler(const char *broker, int port, const char *user, const char *password, bool useTLS);
    void setup(uint8_t ledPWMpin_0, uint8_t ledPWMpin_1, uint8_t ledPWMpin_2, uint8_t ledPWMpin_3, uint8_t ledPWMpin_4);
    void loop();
    void publishDiscoveryMessage(const char *entity, const char *entityReadableName, const char *unit, bool deleteMessage, const char *icon=NULL, const char *deviceClass=NULL, const char *commandEntity=NULL);
    void publishStandardData(String subDevice, String entity, String value, String topicClass="light");
    
    // Setters for runtime configuration
    void setBroker(const char* broker);
    void setPort(int port);
    void setUser(const char* user);
    void setPassword(const char* password);
    void setUseTLS(bool useTLS);
    void setConfiguration(const char *broker, int port, const char *user, const char *password, bool useTLS, const char *sensorUniqueName, const char *mainTopicPath, bool autoDiscovery, const char * ipAddress);
    void setMainTopic(String mainTopicPath);

    void setRemoteDisplayData(boolean remoteDisplayActive);

    void requestMQTTconnectionReset(boolean autoDiscoveryRemoveRequested);

    LedDimmerSet getLedDimmerSet(uint8_t ledNo);
    void stopConnection(boolean full=false);

    static void subscribedMessageArrived(char *topic, byte *payload, unsigned int length);

    boolean setupDone = false;

private:
    const char* mqtt_broker;
    int mqtt_port;
    const char* mqtt_user;
    const char* mqtt_password;
    bool useTLS;
    const char* deviceGroupName;
    const char* espURL;
    String mqttMainTopicPath;
    String gw_ipAddress;
        
    WiFiClient wifiClient;
    WiFiClientSecure wifiClientSecure;
    PubSubClient client;
    
    static MQTTHandler* instance;
   
    boolean autoDiscoveryActive;
    boolean autoDiscoveryActiveRemove;
    boolean requestMQTTconnectionResetFlag;
    unsigned long lastReconnectAttempt = 0;

    // LedDimmerSet lastDimmerSet[LED_DIMMER_COUNT];
    LedDimmerSet lastDimmerSet[5];
    
    void reconnect();
    boolean initiateDiscoveryMessages(bool autoDiscoveryRemove=false);
    void subscribingLEDxLight(uint8_t ledNo);
    void subscribingLEDxNumber(uint8_t ledNo);
};

extern MQTTHandler mqttHandler;

#endif // MQTTHANDLER_H