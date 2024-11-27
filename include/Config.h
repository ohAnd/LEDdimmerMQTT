// Config.h
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#define CONFIG_FILE_PATH "/userconfig.json"

#define LED_DIMMER_COUNT 5 // count of the available dimmer leds

struct ledDimmerConfig
{
    uint8_t ledPWMpin             = 255;   // first init with Pin 4 as base functionality
    uint8_t dimValueStep          = 1;   // in percent 0-100
    uint8_t dimValueStepDelay     = 10;
    uint16_t dimValueRangeLow     = 0;   // 0-1023
    uint16_t dimValueRangeHigh    = 1023;// 0-1023
};

struct UserConfig
{
    char wifiSsid[64]             = "mySSID";
    char wifiPassword[64]         = "myPassword";
    
    char mqttBrokerIpDomain[128]  = "192.168.1.100";
    int mqttBrokerPort            = 1883;
    boolean mqttUseTLS            = false;
    char mqttBrokerUser[64]       = "LEDuser";
    char mqttBrokerPassword[64]   = "LEDpass";
    char mqttBrokerMainTopic[32]  = "LEDdimmerMQTT_12345678";
    boolean mqttHAautoDiscoveryON = false;
    boolean mqttActive            = false;

    // led settings
    ledDimmerConfig ledDimmerConfigs[LED_DIMMER_COUNT];

    boolean wifiAPstart           = true;
    int selectedUpdateChannel     = 0; // 0 - release 1 - snapshot
    int timezoneOffest            = 7200; // default CEST
};

extern UserConfig userConfig;

// Define the UserConfigManager class
class UserConfigManager {
    public:
        UserConfigManager(const char *filePath = CONFIG_FILE_PATH, const UserConfig &defaultConfig = UserConfig());
        bool begin();
        bool loadConfig(UserConfig &config);
        void saveConfig(const UserConfig &config);
        void resetConfig();
        void printConfigdata();
        // String getWebHandler(keyAndValue_t* keyValueWebClient, unsigned int size);
        String getWebHandler(JsonDocument doc);
        

    private:
        const char *filePath;
        UserConfig defaultConfig;
        JsonDocument mappingStructToJson(const UserConfig &config);
        void mappingJsonToStruct(JsonDocument doc);
        String createWebPage(bool updated);
};

extern UserConfigManager configManager;

#endif // CONFIG_H