// Config.cpp
#include "Config.h"

struct UserConfig userConfig;

// Default values
UserConfigManager::UserConfigManager(const char *filePath, const UserConfig &defaultConfig)
    : filePath(filePath), defaultConfig(defaultConfig) {}

bool UserConfigManager::begin()
{
    if (!LittleFS.begin())
    {

        Serial.println(F("UserConfigManager::begin - An error has occurred while mounting LittleFS"));
#if defined(ESP32)
        // specific to ESP32 because it uses the ESP32-specific LittleFS.begin(true) function to format the filesystem if mounting fails.
        if (!LittleFS.begin(true))
        {
            Serial.println(F("... tried to format filesystem also failed."));
        }
        else
        {
            Serial.println(F("... successfully formatted filesystem."));
        }
#endif
        return false;
    }

    UserConfig config;
    if (!loadConfig(config))
    {
        Serial.println(F("UserConfigManager::begin - First run: Initializing config"));
        saveConfig(defaultConfig);
    }
    else
    {
        Serial.println(F("UserConfigManager::begin - Config loaded successfully"));
    }
    return true;
}

bool UserConfigManager::loadConfig(UserConfig &config)
{
    if (!LittleFS.exists(filePath))
    {
        return false;
    }
    File file = LittleFS.open(filePath, "r");
    if (!file)
    {
        Serial.println("UserConfigManager::loadConfig - Failed to open file for reading");
        return false;
    }
    // file.read((uint8_t *)&config, sizeof(UserConfig));
    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, file);
    if (error)
    {
        Serial.print("UserConfigManager::loadConfig - deserializeJson() fehlgeschlagen: ");
        Serial.println(error.c_str());
    }

    mappingJsonToStruct(doc);
    if (userConfig.wifiSsid == "")
    {
        Serial.println(F("UserConfigManager::loadConfig --- ERROR: config corrupted, reset to default"));
        saveConfig(defaultConfig);
    }
    else
    {
        Serial.println("UserConfigManager::loadConfig - config loaded from json: " + String(filePath));
    }

    file.close();
    return true;
}

void UserConfigManager::saveConfig(const UserConfig &config)
{
    File file = LittleFS.open(filePath, "w");
    if (!file)
    {
        Serial.println(F("Failed to open file for writing"));
        return;
    }
    JsonDocument doc;
    doc = mappingStructToJson(config);
    serializeJson(doc, file);

    Serial.println("config saved to json: " + String(filePath));

    file.close();
}

void UserConfigManager::resetConfig()
{
    if (LittleFS.remove(filePath))
    {
        Serial.println(F("Config file deleted successfully"));
    }
    else
    {
        Serial.println(F("Failed to delete config file"));
    }
}

void UserConfigManager::printConfigdata()
{
    // Configuration has been written before
    Serial.print(F("\n--------------------------------------\n"));
    Serial.print(F("Configuration loaded from config file: '/userconfig.json'\n"));
    Serial.print(F("init (wifiAPstart): \t"));
    Serial.println(userConfig.wifiAPstart);

    // wifi settings
    Serial.print(F("wifi ssid: \t\t"));
    Serial.println(userConfig.wifiSsid);
    Serial.print(F("wifi pass: \t\t"));
    Serial.println(userConfig.wifiPassword);

    // led settings
    for (size_t i = 0; i < LED_DIMMER_COUNT; i++)
    {
        Serial.println("\nLED " + String(i) + " setting: \t\t");
        Serial.print(F("nledPWMpin: \t\t"));
        Serial.println(userConfig.ledDimmerConfigs[i].ledPWMpin);
        Serial.print(F("dimValueStep: \t\t"));
        Serial.println(userConfig.ledDimmerConfigs[i].dimValueStep);
        Serial.print(F("dimValueStepDelay: \t"));
        Serial.println(userConfig.ledDimmerConfigs[i].dimValueStepDelay);
        Serial.print(F("dimValueRangeLow: \t"));
        Serial.println(userConfig.ledDimmerConfigs[i].dimValueRangeLow);
        Serial.print(F("dimValueRangeHigh: \t"));
        Serial.println(userConfig.ledDimmerConfigs[i].dimValueRangeHigh);
    }

    // Serial.print(F("\nledPWMpin_0: \t\t"));
    // Serial.println(userConfig.ledPWMpin_0);
    // Serial.print(F("dimValueStep_0: \t\t"));
    // Serial.println(userConfig.dimValueStep_0);
    // Serial.print(F("dimValueStepDelay_0: \t"));
    // Serial.println(userConfig.dimValueStepDelay_0);
    // Serial.print(F("dimValueRangeLow_0: \t"));
    // Serial.println(userConfig.dimValueRangeLow_0);
    // Serial.print(F("dimValueRangeHigh_0: \t"));
    // Serial.println(userConfig.dimValueRangeHigh_0);

    // Serial.print(F("\nledPWMpin_1: \t\t"));
    // Serial.println(userConfig.ledPWMpin_1);
    // Serial.print(F("dimValueStep_1: \t\t"));
    // Serial.println(userConfig.dimValueStep_1);
    // Serial.print(F("dimValueStepDelay_1: \t"));
    // Serial.println(userConfig.dimValueStepDelay_1);
    // Serial.print(F("dimValueRangeLow_1: \t"));
    // Serial.println(userConfig.dimValueRangeLow_1);
    // Serial.print(F("dimValueRangeHigh_1: \t"));
    // Serial.println(userConfig.dimValueRangeHigh_1);

    // Serial.print(F("\nledPWMpin_2: \t\t"));
    // Serial.println(userConfig.ledPWMpin_2);
    // Serial.print(F("dimValueStep_2: \t\t"));
    // Serial.println(userConfig.dimValueStep_2);
    // Serial.print(F("dimValueStepDelay_2: \t"));
    // Serial.println(userConfig.dimValueStepDelay_2);
    // Serial.print(F("dimValueRangeLow_2: \t"));
    // Serial.println(userConfig.dimValueRangeLow_2);
    // Serial.print(F("dimValueRangeHigh_2: \t"));
    // Serial.println(userConfig.dimValueRangeHigh_2);

    // Serial.print(F("\nledPWMpin_3: \t\t"));
    // Serial.println(userConfig.ledPWMpin_3);
    // Serial.print(F("dimValueStep_3: \t\t"));
    // Serial.println(userConfig.dimValueStep_3);
    // Serial.print(F("dimValueStepDelay_3: \t"));
    // Serial.println(userConfig.dimValueStepDelay_3);
    // Serial.print(F("dimValueRangeLow_3: \t"));
    // Serial.println(userConfig.dimValueRangeLow_3);
    // Serial.print(F("dimValueRangeHigh_3: \t"));
    // Serial.println(userConfig.dimValueRangeHigh_3);

    // Serial.print(F("\nledPWMpin_4: \t\t"));
    // Serial.println(userConfig.ledPWMpin_4);
    // Serial.print(F("dimValueStep_4: \t\t"));
    // Serial.println(userConfig.dimValueStep_4);
    // Serial.print(F("dimValueStepDelay_4: \t"));
    // Serial.println(userConfig.dimValueStepDelay_4);
    // Serial.print(F("dimValueRangeLow_4: \t"));
    // Serial.println(userConfig.dimValueRangeLow_4);
    // Serial.print(F("dimValueRangeHigh_4: \t"));
    // Serial.println(userConfig.dimValueRangeHigh_4);

    // mqtt settings
    Serial.print(F("\nmqtt host: \t\t"));
    Serial.println(userConfig.mqttBrokerIpDomain);
    Serial.print(F("mqtt port: \t\t"));
    Serial.println(userConfig.mqttBrokerPort);
    Serial.print(F("mqtt TLS: \t\t"));
    Serial.println(userConfig.mqttUseTLS);
    Serial.print(F("mqtt user: \t\t"));
    Serial.println(userConfig.mqttBrokerUser);
    Serial.print(F("mqtt pass: \t\t"));
    Serial.println(userConfig.mqttBrokerPassword);
    Serial.print(F("mqtt topic: \t\t"));
    Serial.println(userConfig.mqttBrokerMainTopic);
    Serial.print(F("mqtt binding active: \t"));
    Serial.println(userConfig.mqttActive);
    Serial.print(F("mqtt HA autoDiscovery: \t"));
    Serial.println(userConfig.mqttHAautoDiscoveryON);

    Serial.print(F("update channel: \t\t"));
    Serial.println(userConfig.selectedUpdateChannel);

    Serial.print(F("--------------------------------------\n"));
}

JsonDocument UserConfigManager::mappingStructToJson(const UserConfig &config)
{
    JsonDocument doc;

    doc["wifi"]["ssid"] = config.wifiSsid;
    doc["wifi"]["pass"] = config.wifiPassword;

    for (size_t i = 0; i < LED_DIMMER_COUNT; i++)
    {

        doc["led"][i]["ledPWMpin"] = config.ledDimmerConfigs[i].ledPWMpin;
        doc["led"][i]["dimValueStep"] = config.ledDimmerConfigs[i].dimValueStep;
        doc["led"][i]["dimValueStepDelay"] = config.ledDimmerConfigs[i].dimValueStepDelay;
        doc["led"][i]["dimValueRangeLow"] = config.ledDimmerConfigs[i].dimValueRangeLow;
        doc["led"][i]["dimValueRangeHigh"] = config.ledDimmerConfigs[i].dimValueRangeHigh;
    }

    // doc["led"]["ledPWMpin_0"] = config.ledPWMpin_0;
    // doc["led"]["dimValueStep_0"] = config.dimValueStep_0;
    // doc["led"]["dimValueStepDelay_0"] = config.dimValueStepDelay_0;
    // doc["led"]["dimValueRangeLow_0"] = config.dimValueRangeLow_0;
    // doc["led"]["dimValueRangeHigh_0"] = config.dimValueRangeHigh_0;

    // doc["led"]["ledPWMpin_1"] = config.ledPWMpin_1;
    // doc["led"]["dimValueStep_1"] = config.dimValueStep_1;
    // doc["led"]["dimValueStepDelay_1"] = config.dimValueStepDelay_1;
    // doc["led"]["dimValueRangeLow_1"] = config.dimValueRangeLow_1;
    // doc["led"]["dimValueRangeHigh_1"] = config.dimValueRangeHigh_1;

    // doc["led"]["ledPWMpin_2"] = config.ledPWMpin_2;
    // doc["led"]["dimValueStep_2"] = config.dimValueStep_2;
    // doc["led"]["dimValueStepDelay_2"] = config.dimValueStepDelay_2;
    // doc["led"]["dimValueRangeLow_2"] = config.dimValueRangeLow_2;
    // doc["led"]["dimValueRangeHigh_2"] = config.dimValueRangeHigh_2;

    // doc["led"]["ledPWMpin_3"] = config.ledPWMpin_3;
    // doc["led"]["dimValueStep_3"] = config.dimValueStep_3;
    // doc["led"]["dimValueStepDelay_3"] = config.dimValueStepDelay_3;
    // doc["led"]["dimValueRangeLow_3"] = config.dimValueRangeLow_3;
    // doc["led"]["dimValueRangeHigh_3"] = config.dimValueRangeHigh_3;

    // doc["led"]["ledPWMpin_4"] = config.ledPWMpin_4;
    // doc["led"]["dimValueStep_4"] = config.dimValueStep_4;
    // doc["led"]["dimValueStepDelay_4"] = config.dimValueStepDelay_4;
    // doc["led"]["dimValueRangeLow_4"] = config.dimValueRangeLow_4;
    // doc["led"]["dimValueRangeHigh_4"] = config.dimValueRangeHigh_4;

    doc["mqtt"]["active"] = config.mqttActive;
    doc["mqtt"]["brokerIP"] = config.mqttBrokerIpDomain;
    doc["mqtt"]["brokerPort"] = config.mqttBrokerPort;
    doc["mqtt"]["brokerUseTLS"] = config.mqttUseTLS;
    doc["mqtt"]["user"] = config.mqttBrokerUser;
    doc["mqtt"]["pass"] = config.mqttBrokerPassword;
    doc["mqtt"]["mainTopic"] = config.mqttBrokerMainTopic;
    doc["mqtt"]["HAautoDiscoveryON"] = config.mqttHAautoDiscoveryON;

    doc["local"]["selectedUpdateChannel"] = config.selectedUpdateChannel;
    doc["local"]["wifiAPstart"] = config.wifiAPstart;
    doc["local"]["timezoneOffest"] = config.timezoneOffest;

    return doc;
}

void UserConfigManager::mappingJsonToStruct(JsonDocument doc)
{
    String(doc["wifi"]["ssid"].as<String>()).toCharArray(userConfig.wifiSsid, sizeof(userConfig.wifiSsid));
    String(doc["wifi"]["pass"].as<String>()).toCharArray(userConfig.wifiPassword, sizeof(userConfig.wifiPassword));

    for (size_t i = 0; i < LED_DIMMER_COUNT; i++)
    {
        userConfig.ledDimmerConfigs[i].ledPWMpin = doc["led"][i]["ledPWMpin"].as<uint8_t>();
        userConfig.ledDimmerConfigs[i].dimValueStep = doc["led"][i]["dimValueStep"].as<uint8_t>();
        userConfig.ledDimmerConfigs[i].dimValueStepDelay = doc["led"][i]["dimValueStepDelay"].as<uint8_t>();
        userConfig.ledDimmerConfigs[i].dimValueRangeLow = doc["led"][i]["dimValueRangeLow"].as<uint16_t>();
        userConfig.ledDimmerConfigs[i].dimValueRangeHigh = doc["led"][i]["dimValueRangeHigh"].as<uint16_t>();
    }

    // userConfig.ledPWMpin_0 = doc["led"]["ledPWMpin_0"].as<uint8_t>();
    // userConfig.dimValueStep_0 = doc["led"]["dimValueStep_0"].as<uint8_t>();
    // userConfig.dimValueStepDelay_0 = doc["led"]["dimValueStepDelay_0"].as<uint8_t>();
    // userConfig.dimValueRangeLow_0 = doc["led"]["dimValueRangeLow_0"].as<uint16_t>();
    // userConfig.dimValueRangeHigh_0 = doc["led"]["dimValueRangeHigh_0"].as<uint16_t>();

    // userConfig.ledPWMpin_1 = doc["led"]["ledPWMpin_1"].as<uint8_t>();
    // userConfig.dimValueStep_1 = doc["led"]["dimValueStep_1"].as<uint8_t>();
    // userConfig.dimValueStepDelay_1 = doc["led"]["dimValueStepDelay_1"].as<uint8_t>();
    // userConfig.dimValueRangeLow_1 = doc["led"]["dimValueRangeLow_1"].as<uint16_t>();
    // userConfig.dimValueRangeHigh_1 = doc["led"]["dimValueRangeHigh_1"].as<uint16_t>();

    // userConfig.ledPWMpin_2 = doc["led"]["ledPWMpin_2"].as<uint8_t>();
    // userConfig.dimValueStep_2 = doc["led"]["dimValueStep_2"].as<uint8_t>();
    // userConfig.dimValueStepDelay_2 = doc["led"]["dimValueStepDelay_2"].as<uint8_t>();
    // userConfig.dimValueRangeLow_2 = doc["led"]["dimValueRangeLow_2"].as<uint16_t>();
    // userConfig.dimValueRangeHigh_2 = doc["led"]["dimValueRangeHigh_2"].as<uint16_t>();

    // userConfig.ledPWMpin_3 = doc["led"]["ledPWMpin_3"].as<uint8_t>();
    // userConfig.dimValueStep_3 = doc["led"]["dimValueStep_3"].as<uint8_t>();
    // userConfig.dimValueStepDelay_3 = doc["led"]["dimValueStepDelay_3"].as<uint8_t>();
    // userConfig.dimValueRangeLow_3 = doc["led"]["dimValueRangeLow_3"].as<uint16_t>();
    // userConfig.dimValueRangeHigh_3 = doc["led"]["dimValueRangeHigh_3"].as<uint16_t>();

    // userConfig.ledPWMpin_4 = doc["led"]["ledPWMpin_4"].as<uint8_t>();
    // userConfig.dimValueStep_4 = doc["led"]["dimValueStep_4"].as<uint8_t>();
    // userConfig.dimValueStepDelay_4 = doc["led"]["dimValueStepDelay_4"].as<uint8_t>();
    // userConfig.dimValueRangeLow_4 = doc["led"]["dimValueRangeLow_4"].as<uint16_t>();
    // userConfig.dimValueRangeHigh_4 = doc["led"]["dimValueRangeHigh_4"].as<uint16_t>();

    userConfig.mqttActive = doc["mqtt"]["active"].as<bool>();
    String(doc["mqtt"]["brokerIP"].as<String>()).toCharArray(userConfig.mqttBrokerIpDomain, sizeof(userConfig.mqttBrokerIpDomain));
    userConfig.mqttBrokerPort = doc["mqtt"]["brokerPort"].as<int>();
    userConfig.mqttUseTLS = doc["mqtt"]["brokerUseTLS"].as<bool>();
    String(doc["mqtt"]["user"].as<String>()).toCharArray(userConfig.mqttBrokerUser, sizeof(userConfig.mqttBrokerUser));
    String(doc["mqtt"]["pass"].as<String>()).toCharArray(userConfig.mqttBrokerPassword, sizeof(userConfig.mqttBrokerPassword));
    String(doc["mqtt"]["mainTopic"].as<String>()).toCharArray(userConfig.mqttBrokerMainTopic, sizeof(userConfig.mqttBrokerMainTopic));
    userConfig.mqttHAautoDiscoveryON = doc["mqtt"]["HAautoDiscoveryON"].as<bool>();

    userConfig.selectedUpdateChannel = doc["local"]["selectedUpdateChannel"];
    userConfig.wifiAPstart = doc["local"]["wifiAPstart"];
    userConfig.timezoneOffest = doc["local"]["timezoneOffest"];

    return;
}

// reused from https://github.com/Tvde1/ConfigTool/blob/master/src/ConfigTool.cpp

String UserConfigManager::createWebPage(bool updated)
{
    // Serial.println(F("\nCONFIG web - START generate html page for config interface"));

    const String beginHtml = F("<html><head><title>LEDdimmerMQTT Configuration Interface</title><link rel=\"stylesheet\"href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.1.0/css/bootstrap.min.css\"integrity=\"sha384-9gVQ4dYFwwWSjIDZnLEWnxCjeSWFphJiwGPXr1jddIhOegiu1FwO5qRGvFXOdJZ4\"crossorigin=\"anonymous\"><script src=\"https://code.jquery.com/jquery-3.3.1.slim.min.js\"integrity=\"sha384-q8i/X+965DzO0rT7abK41JStQIAqVgRVzpbzo5smXKp4YfRvH+8abtTE1Pi6jizo\"crossorigin=\"anonymous\"></script><script src=\"https://cdnjs.cloudflare.com/ajax/libs/popper.js/1.14.0/umd/popper.min.js\"integrity=\"sha384-cs/chFZiN24E4KMATLdqdvsezGxaGsi4hLGOzlXwp5UZB1LY//20VyM2taTB4QvJ\"crossorigin=\"anonymous\"></script><script src=\"https://stackpath.bootstrapcdn.com/bootstrap/4.1.0/js/bootstrap.min.js\"integrity=\"sha384-uefMccjFJAIv6A+rW+L4AHf99KvxDjWSu1z9VI8SKNVmz4sk7buKt/6v9KI65qnm\"crossorigin=\"anonymous\"></script></head><body><div class=\"container\"><div class=\"jumbotron\"style=\"width:100%\"><h1>LEDdimmerMQTT Configuration Interface</h1><p>Edit the config variables here and click save.<br>After the configuration is saved, a reboot will be triggered. </p></div>");
    const String continueHtml = F("<form method=\"POST\" action=\"\">");
    const String savedAlert = F("<div class=\"alert alert-success\" role=\"alert\"><button type=\"button\" class=\"close\" data-dismiss=\"alert\" aria-label=\"Close\"><span aria-hidden=\"true\">&times;</span></button>The config has been saved. And the device will be rebooted</div>");

    // const String endHtml = "<div class=\"form-group row\"><div class=\"col-sm-1\"><button class=\"btn btn-primary\" type=\"submit\">Save</button></div><div class=\"col-sm-1 offset-sm-0\"><button type=\"button\" class=\"btn btn-danger\" onclick=\"reset()\">Reset</button></div></div></form></div></body><script>function reset(){var url=window.location.href;if(url.indexOf('?')>0){url=url.substring(0,url.indexOf('?'));}url+='?reset=true';window.location.replace(url);}</script></html>";
    const String endHtml = F("<div class=\"form-group row\"><div class=\"col-sm-1\"><button class=\"btn btn-primary\" type=\"submit\">Save</button></div></div></form></div></body><script>function reset(){var url=window.location.href;if(url.indexOf('?')>0){url=url.substring(0,url.indexOf('?'));}url+='?reset=true';window.location.replace(url);}</script></html>");

    String result = beginHtml;

    if (updated)
    {
        result += savedAlert;
    }

    result += continueHtml;

    JsonDocument doc;
    doc = mappingStructToJson(userConfig);

    JsonObject obj = doc.as<JsonObject>();

    for (JsonPair kv : obj)
    {
        String mainKey = String(kv.key().c_str());
        // exception for led iteration needed - json is led.0.ledPWMpin and so on
        if (mainKey == "led")
        {
            for (size_t i = 0; i < LED_DIMMER_COUNT; i++)
            {
                result += "<div><label><h4>LED " + String(i) + "</h4></label></div>";
                JsonObject obj1 = (kv.value()[i]).as<JsonObject>();
                for (JsonPair kv1 : obj1)
                {
                    String key = kv1.key().c_str();
                    String value = kv1.value();
                    result += "<div class=\"form-group row\"><div class=\"col-2\"><label>" + key + "</label></div><div class=\"col-10\"><input name=\"led." + String(i) + "." + key + "\" class=\"form-control\" type=\"text\" value=\"" + value + "\" /></div></div>";
                }
            }
            continue;
        } // end of exception for led iteration
        
        result += "<div><label><h4>" + mainKey + "</h4></label></div>";
        // 1 layer below
        JsonObject obj1 = (kv.value()).as<JsonObject>();
        for (JsonPair kv1 : obj1)
        {
            String key = kv1.key().c_str();
            String value = kv1.value();
            result += "<div class=\"form-group row\"><div class=\"col-2\"><label>" + key + "</label></div><div class=\"col-10\"><input name=\"" + mainKey + "." + key + "\" class=\"form-control\" type=\"text\" value=\"" + value + "\" /></div></div>";
        }
    }

    result += endHtml;

    // Serial.println(F("\nCONFIG web - END generate html page for config interface"));

    return result;
}

String UserConfigManager::getWebHandler(JsonDocument doc)
{
    // JsonDocument docConfigNew;
    bool updated = false;
    if (!doc.isNull())
    {
        File file = LittleFS.open("/userconfig.json", "w");
        if (!file)
        {
            Serial.println(F("Failed to open file for writing"));
            return "<html><body>ERROR - failed to open /userconfig.json</body></html>";
        }
        // Serial.println(F("\nCONFIG web - got new config from web: "));
        // // show the new config
        // serializeJsonPretty(doc, Serial);
        // Serial.println(F("\nCONFIG web - save new config to file"));
        serializeJson(doc, file);
        // serializeJsonPretty(doc, Serial);
        Serial.println("WEBconfig - config saved to json: " + String(filePath));
        file.close();

        loadConfig(userConfig);
        printConfigdata();
        updated = true;
    }
    else
    {
        Serial.println(F("\nCONFIG web - show current config"));
    }

    String html = createWebPage(updated);
    return html;
}