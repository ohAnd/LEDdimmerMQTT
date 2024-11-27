#include <base/webserver.h>

boolean wifiScanIsRunning = false;

size_t content_len;

struct ledDimmerStruct ledDimmerData_0;
struct ledDimmerStruct ledDimmerData_1;
struct ledDimmerStruct ledDimmerData_2;
struct ledDimmerStruct ledDimmerData_3;
struct ledDimmerStruct ledDimmerData_4;

DTUwebserver::DTUwebserver()
{
    // Constructor implementation
}

DTUwebserver::~DTUwebserver()
{
    stop();                  // Ensure the server is stopped and resources are cleaned up
    webServerTimer.detach(); // Stop the timer
}

void DTUwebserver::backgroundTask(DTUwebserver *instance)
{
    if (platformData.rebootRequested)
    {
        if (platformData.rebootRequestedInSec-- == 1)
        {
            platformData.rebootRequestedInSec = 0;
            platformData.rebootRequested = false;
            platformData.rebootStarted = true;
            Serial.println(F("WEB:\t\t backgroundTask - reboot requested"));
            Serial.flush();
            ESP.restart();
        }
        Serial.println("WEB:\t\t backgroundTask - reboot requested with delay - waiting: " + String(platformData.rebootRequestedInSec) + " seconds");
    }
    // if (updateInfo.updateRunning)
    // {
    //     Serial.println("OTA UPDATE:\t Progress: " + String(updateInfo.updateProgress, 1) + " %");
    // }
}

void DTUwebserver::start()
{
    // Initialize the web server and define routes as before
    Serial.println(F("WEB:\t\t setup webserver"));
    // base web pages
    asyncDtuWebServer.on("/", HTTP_GET, handleRoot);
    asyncDtuWebServer.on("/jquery.min.js", HTTP_GET, handleJqueryMinJs);
    asyncDtuWebServer.on("/style.css", HTTP_GET, handleCSS);

    // user config requests
    asyncDtuWebServer.on("/updateWifiSettings", handleUpdateWifiSettings);
    asyncDtuWebServer.on("/updateLedSettings", handleUpdateLedSettings);
    asyncDtuWebServer.on("/updateMqttSettings", handleUpdateMqttSettings);

    // admin config requests
    asyncDtuWebServer.on("/config", handleConfigPage);

    // direct settings
    asyncDtuWebServer.on("/getWifiNetworks", handleGetWifiNetworks);

    // api GETs
    asyncDtuWebServer.on("/api/data.json", handleDataJson);
    asyncDtuWebServer.on("/api/info.json", handleInfojson);

    // OTA direct update
    asyncDtuWebServer.on("/updateOTASettings", handleUpdateOTASettings);
    asyncDtuWebServer.on("/updateGetInfo", handleUpdateInfoRequest);

    // OTA update
    asyncDtuWebServer.on("/doupdate", HTTP_POST, [](AsyncWebServerRequest *request) {}, [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
                         { handleDoUpdate(request, filename, index, data, len, final); });
    asyncDtuWebServer.on("/updateState", HTTP_GET, handleUpdateProgress);

    asyncDtuWebServer.onNotFound(notFound);

    asyncDtuWebServer.begin(); // Start the web server
    webServerTimer.attach(1, DTUwebserver::backgroundTask, this);
#ifdef ESP32
    Update.onProgress(printProgress);
#endif
}

void DTUwebserver::stop()
{
    asyncDtuWebServer.end(); // Stop the web server
    webServerTimer.detach(); // Stop the timer
}

// base pages
void DTUwebserver::handleRoot(AsyncWebServerRequest *request)
{
    request->send_P(200, "text/html", INDEX_HTML);
}
void DTUwebserver::handleCSS(AsyncWebServerRequest *request)
{
    request->send_P(200, "text/html", STYLE_CSS);
}
void DTUwebserver::handleJqueryMinJs(AsyncWebServerRequest *request)
{
    request->send_P(200, "text/html", JQUERY_MIN_JS);
}

// ota update
void DTUwebserver::handleDoUpdate(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
{
    if (!index)
    {
        updateInfo.updateRunning = true;
        updateInfo.updateState = UPDATE_STATE_PREPARE;
        Serial.println("OTA UPDATE:\t Update Start with file: " + filename);
        Serial.println("OTA UPDATE:\t waiting to stop services");
        delay(500);
        Serial.println("OTA UPDATE:\t services stopped - start update");
        content_len = request->contentLength();
        // if filename includes spiffs, update the spiffs partition
        int cmd = (filename.indexOf("spiffs") > -1) ? U_PART : U_FLASH;
#ifdef ESP8266
        Update.runAsync(true);
        if (!Update.begin(content_len, cmd))
        {
#else
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd))
        {
#endif
            Update.printError(Serial);
            updateInfo.updateState = UPDATE_STATE_FAILED;
        }
    }

    if (Update.write(data, len) != len)
    {
        Update.printError(Serial);
        updateInfo.updateState = UPDATE_STATE_FAILED;
#ifdef ESP8266
    }
    else
    {
        updateInfo.updateProgress = (Update.progress() * 100) / Update.size();
        // Serial.println("OTA UPDATE:\t ESP8266 Progress: " + String(updateInfo.updateProgress, 1) + " %");
#endif
    }

    if (final)
    {
        // AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Please wait while the device reboots");
        // response->addHeader("Refresh", "20");
        // response->addHeader("Location", "/");
        // request->send(response);
        if (!Update.end(true))
        {
            Update.printError(Serial);
            updateInfo.updateState = UPDATE_STATE_FAILED;
        }
        else
        {
            Serial.println("OTA UPDATE:\t Update complete");
            updateInfo.updateState = UPDATE_STATE_DONE;
            updateInfo.updateRunning = false;
            Serial.flush();
            platformData.rebootRequestedInSec = 3;
            platformData.rebootRequested = true;
            // delay(1000);
            // ESP.restart();
        }
    }
}
void DTUwebserver::printProgress(size_t prg, size_t sz)
{
    updateInfo.updateProgress = (prg * 100) / content_len;
    // Serial.println("OTA UPDATE:\t ESP32 Progress: " + String(updateInfo.updateProgress, 1) + " %");
}
void DTUwebserver::handleUpdateProgress(AsyncWebServerRequest *request)
{
    String JSON = "{";
    JSON += "\"updateRunning\": " + String(updateInfo.updateRunning) + ",";
    JSON += "\"updateState\": " + String(updateInfo.updateState) + ",";
    JSON += "\"updateProgress\": " + String(updateInfo.updateProgress);
    JSON += "}";
    request->send(200, "application/json", JSON);
}

// admin config
void DTUwebserver::handleConfigPage(AsyncWebServerRequest *request)
{
    JsonDocument doc;
    bool gotUserChanges = false;
    if (request->params() && request->hasParam("local.wifiAPstart", true))
    {
        if (request->getParam("local.wifiAPstart", true)->value() == "false")
        {
            Serial.println(F("WEB:\t\t handleConfigPage - got user changes"));
            gotUserChanges = true;
            for (unsigned int i = 0; i < request->params(); i++)
            {
                String key = request->argName(i);
                String value = request->arg(key);

                // exception for led iteration needed - json is led[0].ledPWMpin and so on
                if (key.indexOf("led.") > -1)
                {
                    String key1 = key.substring(0, key.indexOf("."));
                    int index = key.substring(key.indexOf(".") + 1, key.lastIndexOf(".")).toInt();
                    String key3 = key.substring(key.lastIndexOf(".") + 1);

                    if (value == "false" || value == "true")
                    {
                        bool boolValue = (value == "true");
                        doc[key1][index][key3] = boolValue;
                    }
                    else
                        doc[key1][index][key3] = value;
                    continue;
                }

                String key1 = key.substring(0, key.indexOf("."));
                String key2 = key.substring(key.indexOf(".") + 1);

                if (value == "false" || value == "true")
                {
                    bool boolValue = (value == "true");
                    doc[key1][key2] = boolValue;
                }
                else
                    doc[key1][key2] = value;
            }
        }
    }

    String html = configManager.getWebHandler(doc);
    request->send_P(200, "text/html", html.c_str());
    if (gotUserChanges)
    {
        Serial.println(F("WEB:\t\t handleConfigPage - got User Changes - sent back config page ack - and restart ESP in 2 seconds"));
        platformData.rebootRequestedInSec = 3;
        platformData.rebootRequested = true;
    }
}

// serve json as api
void DTUwebserver::setLEDdata(ledDimmerStruct ledDimmerDataIn[5])
{
    // set led data
    ledDimmerData_0 = ledDimmerDataIn[0];
    ledDimmerData_1 = ledDimmerDataIn[1];
    ledDimmerData_2 = ledDimmerDataIn[2];
    ledDimmerData_3 = ledDimmerDataIn[3];
    ledDimmerData_4 = ledDimmerDataIn[4];
}

String DTUwebserver::ledStateToJSON(ledDimmerStruct ledDimmerData)
{
    String JSON = "";
    JSON = JSON + "{";
    JSON = JSON + "\"mainSwitch\": " + String(ledDimmerData.mainSwitch) + ",";
    JSON = JSON + "\"dimValue\": " + String(ledDimmerData.dimValue) + ",";
    JSON = JSON + "\"dimValueTarget\": " + String(ledDimmerData.dimValueTarget) + ",";
    JSON = JSON + "\"dimValueRaw\": " + String(ledDimmerData.dimValueRaw) + ",";
    JSON = JSON + "\"dimValueStep\": " + String(ledDimmerData.dimValueStep) + ",";
    JSON = JSON + "\"dimValueStepDelay\": " + String(ledDimmerData.dimValueStepDelay);
    JSON = JSON + "}";

    return JSON;
}

String DTUwebserver::ledSettingsToJSON(ledDimmerStruct ledDimmerData)
{
    String JSON = "";
    JSON = JSON + "{";
    JSON = JSON + "\"ledPWMpin\": " + ledDimmerData.ledPWMpin + ",";
    JSON = JSON + "\"dimValueStep\": " + ledDimmerData.dimValueStep + ",";
    JSON = JSON + "\"dimValueStepDelay\": " + ledDimmerData.dimValueStepDelay;
    JSON = JSON + "}";
    return JSON;
}

void DTUwebserver::handleDataJson(AsyncWebServerRequest *request)
{
    String JSON = "{";
    JSON = JSON + "\"ntpStamp\": " + String(platformData.currentNTPtime - userConfig.timezoneOffest) + ",";
    JSON = JSON + "\"starttime\": " + String(platformData.dtuGWstarttime - userConfig.timezoneOffest) + ",";

    JSON = JSON + "\"leds\": [";
    JSON += ledStateToJSON(ledDimmerData_0);
    if (ledDimmerData_1.ledPWMpin != 255)
        JSON += "," + ledStateToJSON(ledDimmerData_1);
    if (ledDimmerData_2.ledPWMpin != 255)
        JSON += "," + ledStateToJSON(ledDimmerData_2);
    if (ledDimmerData_3.ledPWMpin != 255)
        JSON += "," + ledStateToJSON(ledDimmerData_3);
    if (ledDimmerData_4.ledPWMpin != 255)
        JSON += "," + ledStateToJSON(ledDimmerData_4);
    JSON = JSON + "]";
    JSON = JSON + "}";

    request->send(200, "application/json; charset=utf-8", JSON);
}

void DTUwebserver::handleInfojson(AsyncWebServerRequest *request)
{
    String JSON = "{";
    JSON = JSON + "\"chipid\": " + String(platformData.chipID) + ",";
    JSON = JSON + "\"chipType\": \"" + platformData.chipType + "\",";
    JSON = JSON + "\"host\": \"" + platformData.espUniqueName + "\",";
    JSON = JSON + "\"initMode\": " + userConfig.wifiAPstart + ",";

    JSON = JSON + "\"firmware\": {";
    JSON = JSON + "\"version\": \"" + String(platformData.fwVersion) + "\",";
    JSON = JSON + "\"versiondate\": \"" + String(platformData.fwBuildDate) + "\",";
    JSON = JSON + "\"versionServer\": \"" + String(updateInfo.versionServer) + "\",";
    JSON = JSON + "\"versiondateServer\": \"" + String(updateInfo.versiondateServer) + "\",";
    JSON = JSON + "\"versionServerRelease\": \"" + String(updateInfo.versionServerRelease) + "\",";
    JSON = JSON + "\"versiondateServerRelease\": \"" + String(updateInfo.versiondateServerRelease) + "\",";
    JSON = JSON + "\"selectedUpdateChannel\": \"" + String(userConfig.selectedUpdateChannel) + "\",";
    JSON = JSON + "\"updateAvailable\": " + updateInfo.updateAvailable;
    JSON = JSON + "},";

    JSON = JSON + "\"ledSettings\": [";
    JSON += ledSettingsToJSON(ledDimmerData_0);
    if (ledDimmerData_1.ledPWMpin != 255)
        JSON += "," + ledSettingsToJSON(ledDimmerData_1);
    if (ledDimmerData_2.ledPWMpin != 255)
        JSON += "," + ledSettingsToJSON(ledDimmerData_2);
    if (ledDimmerData_3.ledPWMpin != 255)
        JSON += "," + ledSettingsToJSON(ledDimmerData_3);
    if (ledDimmerData_4.ledPWMpin != 255)
        JSON += "," + ledSettingsToJSON(ledDimmerData_4);
    JSON = JSON + "],";

    JSON = JSON + "\"mqttConnection\": {";
    JSON = JSON + "\"mqttActive\": " + userConfig.mqttActive + ",";
    JSON = JSON + "\"mqttIp\": \"" + String(userConfig.mqttBrokerIpDomain) + "\",";
    JSON = JSON + "\"mqttPort\": " + String(userConfig.mqttBrokerPort) + ",";
    JSON = JSON + "\"mqttUseTLS\": " + userConfig.mqttUseTLS + ",";
    JSON = JSON + "\"mqttUser\": \"" + String(userConfig.mqttBrokerUser) + "\",";
    JSON = JSON + "\"mqttPass\": \"" + String(userConfig.mqttBrokerPassword) + "\",";
    JSON = JSON + "\"mqttMainTopic\": \"" + String(userConfig.mqttBrokerMainTopic) + "\",";
    JSON = JSON + "\"mqttHAautoDiscoveryON\": " + userConfig.mqttHAautoDiscoveryON;
    JSON = JSON + "},";

    JSON = JSON + "\"wifiConnection\": {";
    JSON = JSON + "\"wifiSsid\": \"" + String(userConfig.wifiSsid) + "\",";
    JSON = JSON + "\"wifiPassword\": \"" + String(userConfig.wifiPassword) + "\",";
    JSON = JSON + "\"rssiGW\": \"" + String(platformData.wifiRSSI) + "\",";
    JSON = JSON + "\"wifiScanIsRunning\": " + wifiScanIsRunning + ",";
    JSON = JSON + "\"networkCount\": " + platformData.wifiNetworkCount + ",";
    JSON = JSON + "\"foundNetworks\":" + platformData.wifiFoundNetworks;
    JSON = JSON + "}";

    JSON = JSON + "}";

    request->send(200, "application/json; charset=utf-8", JSON);
}

// user config
void DTUwebserver::handleUpdateWifiSettings(AsyncWebServerRequest *request)
{
    if (request->hasParam("wifiSSIDsend", true) && request->hasParam("wifiPASSsend", true))
    {
        String wifiSSIDUser = request->getParam("wifiSSIDsend", true)->value(); // server.arg("wifiSSIDsend"); // retrieve message from webserver
        String wifiPassUser = request->getParam("wifiPASSsend", true)->value(); // server.arg("wifiPASSsend"); // retrieve message from webserver
        Serial.println("WEB:\t\t handleUpdateWifiSettings - got WifiSSID: " + wifiSSIDUser + " - got WifiPass: " + wifiPassUser);

        wifiSSIDUser.toCharArray(userConfig.wifiSsid, sizeof(userConfig.wifiSsid));
        wifiPassUser.toCharArray(userConfig.wifiPassword, sizeof(userConfig.wifiPassword));

        // after saving from user entry - no more in init state
        if (userConfig.wifiAPstart)
        {
            userConfig.wifiAPstart = false;

            // and schedule a reboot to start fresh with new settings
            platformData.rebootRequestedInSec = 2;
            platformData.rebootRequested = true;
        }

        configManager.saveConfig(userConfig);

        String JSON = "{";
        JSON = JSON + "\"wifiSSIDUser\": \"" + userConfig.wifiSsid + "\",";
        JSON = JSON + "\"wifiPassUser\": \"" + userConfig.wifiPassword + "\",";
        JSON = JSON + "}";

        request->send(200, "application/json", JSON);

        // reconnect with new values
        // WiFi.disconnect();
        // WiFi.mode(WIFI_STA);
        // checkWifiTask();

        Serial.println("WEB:\t\t handleUpdateWifiSettings - send JSON: " + String(JSON));
    }
    else
    {
        request->send(400, "text/plain", "handleUpdateWifiSettings - ERROR requested without the expected params");
        Serial.println(F("handleUpdateWifiSettings - ERROR without the expected params"));
    }
}

void DTUwebserver::handleUpdateLedSettings(AsyncWebServerRequest *request)
{
    if (request->hasParam("dimValueStep_0_Send", true) &&
        request->hasParam("dimValueStepDelay_0_Send", true))
    {
        // String dimValueStep_0_Send = request->getParam("dimValueStep_0_Send", true)->value();           // retrieve message from webserver
        // String dimValueStepDelay_0_Send = request->getParam("dimValueStepDelay_0_Send", true)->value(); // retrieve message from webserver
        // Serial.println("WEB:\t\t handleUpdateLedSettings - got dimValueStep: " + dimValueStep_0_Send + "- got dimValueStepDelay: " + dimValueStepDelay_0_Send);

        for (u8_t i = 0; i < LED_DIMMER_COUNT; i++)
        {
            String param1 = "dimValueStep_" + String(i) + "_Send";
            String param2 = "dimValueStepDelay_" + String(i) + "_Send";
            if (request->hasParam(param1, true) && request->hasParam(param2, true))
            {
                String dimValueStep_x_Send = request->getParam(param1, true)->value();           // retrieve message from webserver
                String dimValueStepDelay_x_Send = request->getParam(param2, true)->value(); // retrieve message from webserver

                Serial.println("WEB:\t\t handleUpdateLedSettings - got for LED " + String(i) + " dimValueStep: " + dimValueStep_x_Send + "- got dimValueStepDelay: " + dimValueStepDelay_x_Send);

                userConfig.ledDimmerConfigs[i].dimValueStep = dimValueStep_x_Send.toInt();
                userConfig.ledDimmerConfigs[i].dimValueStepDelay = dimValueStepDelay_x_Send.toInt();
            }
        }

        // userConfig.dimValueStep_0 = dimValueStep_0_Send.toInt();
        // userConfig.dimValueStepDelay_0 = dimValueStepDelay_0_Send.toInt();

        // if (request->hasParam("dimValueStep_1_Send", true) && request->hasParam("dimValueStepDelay_1_Send", true))
        // {
        //     userConfig.dimValueStep_1 = request->getParam("dimValueStep_1_Send", true)->value().toInt();
        //     userConfig.dimValueStepDelay_1 = request->getParam("dimValueStepDelay_1_Send", true)->value().toInt();
        // }

        // if (request->hasParam("dimValueStep_2_Send", true) && request->hasParam("dimValueStepDelay_2_Send", true))
        // {
        //     userConfig.dimValueStep_2 = request->getParam("dimValueStep_2_Send", true)->value().toInt();
        //     userConfig.dimValueStepDelay_2 = request->getParam("dimValueStepDelay_2_Send", true)->value().toInt();
        // }

        // if (request->hasParam("dimValueStep_3_Send", true) && request->hasParam("dimValueStepDelay_3_Send", true))
        // {
        //     userConfig.dimValueStep_3 = request->getParam("dimValueStep_3_Send", true)->value().toInt();
        //     userConfig.dimValueStepDelay_3 = request->getParam("dimValueStepDelay_3_Send", true)->value().toInt();
        // }

        // if (request->hasParam("dimValueStep_4_Send", true) && request->hasParam("dimValueStepDelay_4_Send", true))
        // {
        //     userConfig.dimValueStep_4 = request->getParam("dimValueStep_4_Send", true)->value().toInt();
        //     userConfig.dimValueStepDelay_4 = request->getParam("dimValueStepDelay_4_Send", true)->value().toInt();
        // }

        configManager.saveConfig(userConfig);

        String JSON = "{";
        for (u8_t i = 0; i < LED_DIMMER_COUNT; i++)
        {
            JSON = JSON + "\"dimValueStep_" + String(i) + "\": " + String(userConfig.ledDimmerConfigs[i].dimValueStep) + ",";
            JSON = JSON + "\"dimValueStepDelay_" + String(i) + "\": " + String(userConfig.ledDimmerConfigs[i].dimValueStepDelay);
            if (i < LED_DIMMER_COUNT - 1)
                JSON = JSON + ",";
        }
        JSON = JSON + "}";

        request->send(200, "application/json", JSON);

        Serial.println("WEB:\t\t handleUpdateLedSettings - send JSON: " + String(JSON));
    }
    else
    {
        request->send(400, "text/plain", "handleUpdateLedSettings - ERROR requested without the expected params");
        Serial.println(F("WEB:\t\t handleUpdateLedSettings - ERROR without the expected params"));
    }
}

void DTUwebserver::handleUpdateMqttSettings(AsyncWebServerRequest *request)
{
    if (request->hasParam("mqttIpSend", true) &&
        request->hasParam("mqttPortSend", true) &&
        request->hasParam("mqttUserSend", true) &&
        request->hasParam("mqttPassSend", true) &&
        request->hasParam("mqttMainTopicSend", true) &&
        request->hasParam("mqttActiveSend", true) &&
        request->hasParam("mqttUseTLSSend", true) &&
        request->hasParam("mqttHAautoDiscoveryONSend", true))
    {
        String mqttIP = request->getParam("mqttIpSend", true)->value();
        String mqttPort = request->getParam("mqttPortSend", true)->value();
        String mqttUser = request->getParam("mqttUserSend", true)->value();
        String mqttPass = request->getParam("mqttPassSend", true)->value();
        String mqttMainTopic = request->getParam("mqttMainTopicSend", true)->value();
        String mqttActive = request->getParam("mqttActiveSend", true)->value();
        String mqttUseTLS = request->getParam("mqttUseTLSSend", true)->value();
        String mqttHAautoDiscoveryON = request->getParam("mqttHAautoDiscoveryONSend", true)->value();

        bool mqttHAautoDiscoveryONlastState = userConfig.mqttHAautoDiscoveryON;
        Serial.println("WEB:\t\t handleUpdateMqttSettings - HAautoDiscovery current state: " + String(mqttHAautoDiscoveryONlastState));

        mqttIP.toCharArray(userConfig.mqttBrokerIpDomain, sizeof(userConfig.mqttBrokerIpDomain));
        userConfig.mqttBrokerPort = mqttPort.toInt();
        mqttUser.toCharArray(userConfig.mqttBrokerUser, sizeof(userConfig.mqttBrokerUser));
        mqttPass.toCharArray(userConfig.mqttBrokerPassword, sizeof(userConfig.mqttBrokerPassword));
        mqttMainTopic.toCharArray(userConfig.mqttBrokerMainTopic, sizeof(userConfig.mqttBrokerMainTopic));

        if (mqttActive == "1")
            userConfig.mqttActive = true;
        else
            userConfig.mqttActive = false;

        if (mqttUseTLS == "1")
            userConfig.mqttUseTLS = true;
        else
            userConfig.mqttUseTLS = false;

        if (mqttHAautoDiscoveryON == "1")
            userConfig.mqttHAautoDiscoveryON = true;
        else
            userConfig.mqttHAautoDiscoveryON = false;

        configManager.saveConfig(userConfig);

        if (userConfig.mqttActive)
        {
            // changing to given mqtt setting - inlcuding reset the connection
            // mqttHandler.setConfiguration(userConfig.mqttBrokerIpDomain, userConfig.mqttBrokerPort, userConfig.mqttBrokerUser, userConfig.mqttBrokerPassword, userConfig.mqttUseTLS, (platformData.espUniqueName).c_str(), userConfig.mqttBrokerMainTopic, userConfig.mqttHAautoDiscoveryON, ((platformData.dtuGatewayIP).toString()).c_str());

            Serial.println("WEB:\t\t handleUpdateMqttSettings - HAautoDiscovery new state: " + String(userConfig.mqttHAautoDiscoveryON));
            // mqttHAautoDiscoveryON going from on to off - send one time the delete messages
            if (!userConfig.mqttHAautoDiscoveryON && mqttHAautoDiscoveryONlastState)
                mqttHandler.requestMQTTconnectionReset(true);
            else
                mqttHandler.requestMQTTconnectionReset(false);

            // after changing of auto discovery stop connection to initiate takeover of new settings
            // mqttHandler.stopConnection();

            platformData.rebootRequestedInSec = 3;
            platformData.rebootRequested = true;
        }

        String JSON = "{";
        JSON = JSON + "\"mqttActive\": " + userConfig.mqttActive + ",";
        JSON = JSON + "\"mqttBrokerIpDomain\": \"" + userConfig.mqttBrokerIpDomain + "\",";
        JSON = JSON + "\"mqttBrokerPort\": " + String(userConfig.mqttBrokerPort) + ",";
        JSON = JSON + "\"mqttBrokerIpDomainPort\": \"" + userConfig.mqttBrokerIpDomain + ":" + String(userConfig.mqttBrokerPort) + "\",";
        JSON = JSON + "\"mqttUseTLS\": " + userConfig.mqttUseTLS + ",";
        JSON = JSON + "\"mqttBrokerUser\": \"" + userConfig.mqttBrokerUser + "\",";
        JSON = JSON + "\"mqttBrokerPassword\": \"" + userConfig.mqttBrokerPassword + "\",";
        JSON = JSON + "\"mqttBrokerMainTopic\": \"" + userConfig.mqttBrokerMainTopic + "\",";
        JSON = JSON + "\"mqttHAautoDiscoveryON\": " + userConfig.mqttHAautoDiscoveryON;
        JSON = JSON + "}";

        request->send(200, "application/json", JSON);
        Serial.println("WEB:\t\t handleUpdateMqttSettings - send JSON: " + String(JSON));
    }
    else
    {
        request->send(400, "text/plain", "handleUpdateMqttSettings - ERROR request without the expected params");
        Serial.println(F("WEB:\t\t handleUpdateMqttSettings - ERROR without the expected params"));
    }
}

// direct wifi scan
void DTUwebserver::setWifiScanIsRunning(bool state)
{
    wifiScanIsRunning = state;
}

void DTUwebserver::handleGetWifiNetworks(AsyncWebServerRequest *request)
{
    if (!wifiScanIsRunning)
    {
        WiFi.scanNetworks(true);

        request->send(200, "application/json", "{\"wifiNetworks\": \"scan started\"}");
        Serial.println(F("DTUwebserver:\t -> WIFI_SCAN: start async scan"));
        wifiScanIsRunning = true;
    }
    else
    {
        request->send(200, "application/json", "{\"wifiNetworks\": \"scan already running\"}");
        Serial.println(F("DTUwebserver:\t -> WIFI_SCAN: scan already running"));
    }
}

// OTA settings
void DTUwebserver::handleUpdateOTASettings(AsyncWebServerRequest *request)
{
    if (request->hasParam("releaseChannel", true))
    {
        String releaseChannel = request->getParam("releaseChannel", true)->value(); // retrieve message from webserver
        Serial.println("WEB:\t\t handleUpdateOTASettings - got releaseChannel: " + releaseChannel);

        userConfig.selectedUpdateChannel = releaseChannel.toInt();

        //   configManager.saveConfig(userConfig);

        String JSON = "{";
        JSON = JSON + "\"releaseChannel\": \"" + userConfig.selectedUpdateChannel + "\"";
        JSON = JSON + "}";

        request->send(200, "application/json", JSON);
        Serial.println("WEB:\t\t handleUpdateOTASettings - send JSON: " + String(JSON));

        // trigger new update info with changed release channel
        // getUpdateInfo(AsyncWebServerRequest *request);
        updateInfo.updateInfoRequested = true;
    }
    else
    {
        request->send(400, "text/plain", "handleUpdateOTASettings - ERROR requested without the expected params");
        Serial.println(F("WEB:\t\t handleUpdateOTASettings - ERROR without the expected params"));
    }
}

void DTUwebserver::handleUpdateInfoRequest(AsyncWebServerRequest *request)
{
    updateInfo.updateInfoRequested = true;
    request->send(200, "application/json", "{\"updateInfo.updateInfoRequested\": \"done\"}");
}

// default

void DTUwebserver::notFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Not found");
}
