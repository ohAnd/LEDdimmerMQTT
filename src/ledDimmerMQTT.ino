#if defined(ESP8266)
// #define HARDWARE "ESP8266"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#elif defined(ESP32)
// #define HARDWARE "ESP32"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <map>
#endif

#include <WiFiUdp.h>
#include <NTPClient.h>
#include <UnixTime.h>

#include <dimmerLed.h>

#include <base/webserver.h>
#include <base/platformData.h>

#include <mqttHandler.h>

#include "Config.h"

// ---> START initializing here and publishishing allover project over platformData.h
baseDataStruct platformData;

baseUpdateInfoStruct updateInfo;

const long interval1ms = 1;       // interval (milliseconds)
const long interval10ms = 10;     // interval (milliseconds)
const long interval50ms = 50;     // interval (milliseconds)
const long interval100ms = 100;   // interval (milliseconds)
const long intervalShort = 1000;  // interval (milliseconds)
const long interval5000ms = 5000; // interval (milliseconds)
const long interval30000ms = 30000;
const long intervalLong = 60000; // interval (milliseconds)
unsigned long previousMillis1ms = 0;
unsigned long previousMillis10ms = 0;
unsigned long previousMillis50ms = 0;
unsigned long previousMillis100ms = 0;
unsigned long previousMillisShort = 1704063600;   // in seconds
unsigned long previousMillis5000ms = 1704063600;  // in seconds
unsigned long previousMillis30000ms = 1704063600; // in seconds
unsigned long previousMillisLong = 1704063600;

#define WIFI_RETRY_TIME_SECONDS 30
#define WIFI_RETRY_TIMEOUT_SECONDS 15
#define RECONNECTS_ARRAY_SIZE 50
unsigned long reconnects[RECONNECTS_ARRAY_SIZE];
int reconnectsCnt = -1; // first needed run inkrement to 0

struct controls
{
  boolean wifiSwitch = true;
  boolean getDataAuto = true;
  boolean getDataOnce = false;
  boolean dataFormatJSON = false;
};
controls globalControls;

// wifi functions
boolean wifi_connecting = false;
int wifiTimeoutShort = WIFI_RETRY_TIMEOUT_SECONDS;
int wifiTimeoutLong = WIFI_RETRY_TIME_SECONDS;

// <--- END initializing here and published over platformData.h

// blink code for status display
// defaults
#define LED_BLINK 2
#define LED_BLINK_ON LOW
#define LED_BLINK_OFF HIGH

#if defined(ESP8266)
#warning "Compiling for ESP8266"
#elif CONFIG_IDF_TARGET_ESP32
#undef LED_BLINK
#undef LED_BLINK_ON
#undef LED_BLINK_OFF
#define LED_BLINK 2
#define LED_BLINK_ON HIGH
#define LED_BLINK_OFF LOW
#warning "Compiling for ESP32"
#elif CONFIG_IDF_TARGET_ESP32S2
#undef LED_BLINK
#undef LED_BLINK_ON
#undef LED_BLINK_OFF
#define LED_BLINK 15
#define LED_BLINK_ON HIGH
#define LED_BLINK_OFF LOW
#warning "Compiling for ESP32S2"
#endif

#define BLINK_NORMAL_CONNECTION 0    // 1 Hz blip - normal connection and running
#define BLINK_WAITING_NEXT_TRY_DTU 1 // 1 Hz - waiting for next try to connect to DTU
#define BLINK_WIFI_OFF 2             // 2 Hz - wifi off
#define BLINK_TRY_CONNECT_DTU 3      // 5 Hz - try to connect to DTU
#define BLINK_PAUSE_CLOUD_UPDATE 4   // 0,5 Hz blip - DTO - Cloud update
int8_t blinkCode = BLINK_WIFI_OFF;

// user config
UserConfigManager configManager;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP); // By default 'pool.ntp.org' is used with 60 seconds update interval

DTUwebserver dtuWebServer;

MQTTHandler mqttHandler(userConfig.mqttBrokerIpDomain, userConfig.mqttBrokerPort, userConfig.mqttBrokerUser, userConfig.mqttBrokerPassword, userConfig.mqttUseTLS);

DimmerLed dimmerLedArray[LED_DIMMER_COUNT];

boolean checkWifiTask()
{
  if (WiFi.status() != WL_CONNECTED && !wifi_connecting) // start connecting wifi
  {
    // reconnect counter - and reset to default
    reconnects[reconnectsCnt++] = platformData.currentNTPtime;
    if (reconnectsCnt >= 25)
    {
      reconnectsCnt = 0;
      Serial.println(F("CheckWifi:\t  no Wifi connection after 25 tries!"));
      // after 20 reconnects inner 7 min - write defaults
      if ((platformData.currentNTPtime - reconnects[0]) < (WIFI_RETRY_TIME_SECONDS * 1000)) //
      {
        Serial.println(F("CheckWifi:\t no Wifi connection after 5 tries and inner 5 minutes"));
      }
    }

    // try to connect with current values
    Serial.println("CheckWifi:\t No Wifi connection! Connecting... try to connect to wifi: '" + String(userConfig.wifiSsid) + "' with pass: '" + userConfig.wifiPassword + "'");

    WiFi.disconnect();
    WiFi.begin(userConfig.wifiSsid, userConfig.wifiPassword);
    wifi_connecting = true;
    blinkCode = BLINK_TRY_CONNECT_DTU;

    return false;
  }
  else if (WiFi.status() != WL_CONNECTED && wifi_connecting && wifiTimeoutShort > 0) // check during connecting wifi and decrease for short timeout
  {
    // Serial.printf("CheckWifi:\t connecting - timeout: %i ", wifiTimeoutShort);
    // Serial.print(".");
    wifiTimeoutShort--;
    if (wifiTimeoutShort == 0)
    {
      Serial.println("CheckWifi:\t still no Wifi connection - next try in " + String(wifiTimeoutLong) + " seconds (current retry count: " + String(reconnectsCnt) + ")");
      WiFi.disconnect();
      blinkCode = BLINK_WAITING_NEXT_TRY_DTU;
    }
    return false;
  }
  else if (WiFi.status() != WL_CONNECTED && wifi_connecting && wifiTimeoutShort == 0 && wifiTimeoutLong-- <= 0) // check during connecting wifi and decrease for short timeout
  {
    Serial.println(F("CheckWifi:\t state 'connecting' - wait time done"));
    wifiTimeoutShort = WIFI_RETRY_TIMEOUT_SECONDS;
    wifiTimeoutLong = WIFI_RETRY_TIME_SECONDS;
    wifi_connecting = false;
    return false;
  }
  else if (WiFi.status() == WL_CONNECTED && wifi_connecting) // is connected after connecting
  {
    Serial.println(F("CheckWifi:\t is now connected after state: 'connecting'"));
    wifi_connecting = false;
    wifiTimeoutShort = WIFI_RETRY_TIMEOUT_SECONDS;
    wifiTimeoutLong = WIFI_RETRY_TIME_SECONDS;
    startServices();
    return true;
  }
  else if (WiFi.status() == WL_CONNECTED) // everything fine & connected
  {
    // Serial.println(F("CheckWifi:\t Wifi connection: checked and fine ..."));
    blinkCode = BLINK_NORMAL_CONNECTION;
    return true;
  }
  else
  {
    return false;
  }
}

// scan network for first settings or change
boolean scanNetworksResult()
{
  int networksFound = WiFi.scanComplete();
  // print out Wi-Fi network scan result upon completion
  if (networksFound > 0)
  {
    Serial.print(F("WIFI_SCAN:\t done: "));
    Serial.println(String(networksFound) + " wifi's found");
    platformData.wifiNetworkCount = networksFound;
    platformData.wifiFoundNetworks = "[";
    for (int i = 0; i < networksFound; i++)
    {
      int wifiPercent = 2 * (WiFi.RSSI(i) + 100);
      if (wifiPercent > 100)
      {
        wifiPercent = 100;
      }
      // Serial.printf("%d: %s, Ch:%d (%ddBm, %d) %s\n", i + 1, WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i), wifiPercent, WiFi.encryptionType(i) == ENC_TYPE_NONE ? "open" : "");
      platformData.wifiFoundNetworks = platformData.wifiFoundNetworks + "{\"name\":\"" + WiFi.SSID(i).c_str() + "\",\"wifi\":" + wifiPercent + ",\"rssi\":" + WiFi.RSSI(i) + ",\"chan\":" + WiFi.channel(i) + "}";
      if (i < networksFound - 1)
      {
        platformData.wifiFoundNetworks = platformData.wifiFoundNetworks + ",";
      }
    }
    platformData.wifiFoundNetworks = platformData.wifiFoundNetworks + "]";
    WiFi.scanDelete();
    dtuWebServer.setWifiScanIsRunning(false);
    return true;
  }
  else
  {
    // Serial.println(F("no networks found after scanning!"));
    return false;
  }
}

String getTimeStringByTimestamp(unsigned long timestamp)
{
  UnixTime stamp(1);
  char buf[31];
  stamp.getDateTime(timestamp - 3600);
  // should have the format "2023-11-11T18:11:17+00:00"
  snprintf(buf, sizeof(buf), "%04i-%02i-%02iT%02i:%02i:%02i%+03i:00", stamp.year, stamp.month, stamp.day, stamp.hour, stamp.minute, stamp.second, userConfig.timezoneOffest / 3600);
  return String(buf);
}

void setConfigValuesToDimmer()
{
  for (size_t i = 0; i < LED_DIMMER_COUNT; i++)
  {
    dimmerLedArray[i].setConfigValues(userConfig.ledDimmerConfigs[i].dimValueStep, userConfig.ledDimmerConfigs[i].dimValueStepDelay, userConfig.ledDimmerConfigs[i].dimValueRangeLow, userConfig.ledDimmerConfigs[i].dimValueRangeHigh);
  }
}

// mqtt client - publishing data in standard or HA mqtt auto discovery format
void updateValuesToMqttLED(ledDimmerStruct currentDimValues, String subDevice, boolean doUpdate = false)
{
  uint16_t maxTransitionTime = currentDimValues.dimValueStepDelay * currentDimValues.dimValueStep * (currentDimValues.dimValueRangeHigh - currentDimValues.dimValueRangeLow) * 2.5;

  // if there is no update to mqtt needed return
  if (!currentDimValues.inTransition && (millis() - currentDimValues.dimTargetStartTimestamp > maxTransitionTime) && !doUpdate)
    return;
  // else if(subDevice == "led3")
  //   Serial.println("MQTT:\t\t maxTransitionTime: " + String(maxTransitionTime) + " - currentDimValues.inTransition: " + String(currentDimValues.inTransition) + " - diff since change: " + String(millis() - currentDimValues.dimTargetStartTimestamp));

  // Serial.println("MQTT:\t\t publish data");
  // led data
  mqttHandler.publishStandardData(subDevice, "dimmer", String(currentDimValues.dimValue).c_str());
  mqttHandler.publishStandardData(subDevice, "switch", currentDimValues.mainSwitch ? "ON" : "OFF");
  mqttHandler.publishStandardData(subDevice + "_dimValueStep", "", String(currentDimValues.dimValueStep).c_str(), "number");
  mqttHandler.publishStandardData(subDevice + "_dimValueStepDelay", "", String(currentDimValues.dimValueStepDelay).c_str(), "number");
  mqttHandler.publishStandardData(subDevice, "ledPWMpin", String(currentDimValues.ledPWMpin).c_str());
  // main data
  mqttHandler.publishStandardData("timestamp", "", getTimeStringByTimestamp(platformData.currentNTPtime).c_str(), "sensor");
}

void updateValuesToMqtt()
{
  for (int i = 0; i < LED_DIMMER_COUNT; i++)
  {
    ledDimmerStruct currentDimValues = dimmerLedArray[i].getDimmerValues();
    if (currentDimValues.ledPWMpin != 255)
      updateValuesToMqttLED(currentDimValues, "led" + String(i));
  }
}

// ****

void setup()
{
// switch off SCK LED
// pinMode(14, OUTPUT);
// digitalWrite(14, LOW);

// shortend chip id for ESP32  based on MAC - to be compliant with ESP8266 ESP.getChipId() output
#if defined(ESP32)
  platformData.chipID = 0;
  for (int i = 0; i < 17; i = i + 8)
  {
    platformData.chipID |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  platformData.espUniqueName = String(AP_NAME_START) + "_" + platformData.chipID;
  platformData.esp32 = true;
#endif

  // initialize digital pin LED_BLINK as an output.
  pinMode(LED_BLINK, OUTPUT);
  digitalWrite(LED_BLINK, LED_BLINK_OFF); // turn the LED off by making the voltage LOW

  Serial.begin(115200);
  Serial.print(F("\n\nBooting - with firmware version "));
  Serial.println(platformData.fwVersion);
  Serial.println(F("------------------------------------------------------------------"));

  if (!configManager.begin())
  {
    Serial.println(F("Failed to initialize UserConfigManager"));
    return;
  }

  if (configManager.loadConfig(userConfig))
    configManager.printConfigdata();
  else
    Serial.println(F("Failed to load user config"));
  // ------- user config loaded --------------------------------------------

  if (userConfig.wifiAPstart)
  {
    Serial.println(F("\n+++ device in 'first start' mode - have to be initialized over own served wifi +++\n"));

    WiFi.scanNetworks();
    scanNetworksResult();

    // Connect to Wi-Fi as AP
    WiFi.mode(WIFI_AP);
    WiFi.softAP(platformData.espUniqueName);
    Serial.println("\n +++ serving access point with SSID: '" + platformData.espUniqueName + "' +++\n");

    // IP Address of the ESP8266 on the AP network
    IPAddress apIP = WiFi.softAPIP();
    Serial.print(F("AP IP address: "));
    Serial.println(apIP);

    MDNS.begin(platformData.espUniqueName);
    MDNS.addService("http", "tcp", 80);
    Serial.println("Ready! Open http://" + platformData.espUniqueName + ".local in your browser");

    // default setting for mqtt main topic
    ("LEDdimmerMQTT_" + String(platformData.chipID)).toCharArray(userConfig.mqttBrokerMainTopic, sizeof(userConfig.mqttBrokerMainTopic));
    configManager.saveConfig(userConfig);

    dtuWebServer.start();
  }
  else
  {
    WiFi.mode(WIFI_STA);
  }

  for (int i = 0; i < LED_DIMMER_COUNT; i++)
  {
    dimmerLedArray[i].setup(userConfig.ledDimmerConfigs[i].ledPWMpin);
  }

  setConfigValuesToDimmer();

  // delay for startup background tasks in ESP
  delay(2000);
}

// after startup or reconnect with wifi
void startServices()
{
  if (WiFi.waitForConnectResult() == WL_CONNECTED)
  {
    Serial.print(F("WIFIclient:\t connected! IP address: "));
    platformData.dtuGatewayIP = WiFi.localIP();
    Serial.println((platformData.dtuGatewayIP).toString());
    Serial.print(F("WIFIclient:\t IP address of gateway: "));
    Serial.println(WiFi.gatewayIP());

    MDNS.begin(platformData.espUniqueName);
    MDNS.addService("http", "tcp", 80);
    Serial.println("MDNS:\t\t ready! Open http://" + platformData.espUniqueName + ".local in your browser");

    // ntp time - offset in summertime 7200 else 3600
    timeClient.begin();
    timeClient.setTimeOffset(userConfig.timezoneOffest);
    // get first time
    timeClient.update();
    platformData.dtuGWstarttime = timeClient.getEpochTime();
    Serial.print(F("NTPclient:\t got time from time server: "));
    Serial.println(String(platformData.dtuGWstarttime));

    dtuWebServer.start();

    mqttHandler.setConfiguration(userConfig.mqttBrokerIpDomain, userConfig.mqttBrokerPort, userConfig.mqttBrokerUser, userConfig.mqttBrokerPassword, userConfig.mqttUseTLS, (platformData.espUniqueName).c_str(), userConfig.mqttBrokerMainTopic, userConfig.mqttHAautoDiscoveryON, ((platformData.dtuGatewayIP).toString()).c_str());
    mqttHandler.setup(userConfig.ledDimmerConfigs[0].ledPWMpin, userConfig.ledDimmerConfigs[1].ledPWMpin, userConfig.ledDimmerConfigs[2].ledPWMpin, userConfig.ledDimmerConfigs[3].ledPWMpin, userConfig.ledDimmerConfigs[4].ledPWMpin);
  }
  else
  {
    Serial.println(F("WIFIclient:\t connection failed"));
  }
}

uint16_t ledCycle = 0;
void blinkCodeTask()
{
  int8_t ledOffCount = 2;
  int8_t ledOffReset = 11;

  ledCycle++;
  if (blinkCode == BLINK_NORMAL_CONNECTION) // Blip every 5 sec
  {
    ledOffCount = 2;  // 200 ms
    ledOffReset = 50; // 5000 ms
  }
  else if (blinkCode == BLINK_WAITING_NEXT_TRY_DTU) // 0,5 Hz
  {
    ledOffCount = 10; // 1000 ms
    ledOffReset = 20; // 2000 ms
  }
  else if (blinkCode == BLINK_WIFI_OFF) // long Blip every 5 sec
  {
    ledOffCount = 5;  // 500 ms
    ledOffReset = 50; // 5000 ms
  }
  else if (blinkCode == BLINK_TRY_CONNECT_DTU) // 5 Hz
  {
    ledOffCount = 2; // 200 ms
    ledOffReset = 2; // 200 ms
  }
  else if (blinkCode == BLINK_PAUSE_CLOUD_UPDATE) // Blip every 2 sec
  {
    ledOffCount = 2;  // 200 ms
    ledOffReset = 21; // 2000 ms
  }

  if (ledCycle == 1)
  {
    digitalWrite(LED_BLINK, LED_BLINK_ON); // turn the LED on
  }
  else if (ledCycle == ledOffCount)
  {
    digitalWrite(LED_BLINK, LED_BLINK_OFF); // turn the LED off
  }
  if (ledCycle >= ledOffReset)
  {
    ledCycle = 0;
  }
}

// serial comm
String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++)
  {
    if (data.charAt(i) == separator || i == maxIndex)
    {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void serialInputTask()
{
  // Check to see if anything is available in the serial receive buffer
  if (Serial.available() > 0)
  {
    static char message[20];
    static unsigned int message_pos = 0;
    char inByte = Serial.read();
    if (inByte != '\n' && (message_pos < 20 - 1))
    {
      message[message_pos] = inByte;
      message_pos++;
    }
    else // Full message received...
    {
      // Add null character to string
      message[message_pos] = '\0';
      // Print the message (or do other things)
      Serial.print(F("GotCmd: "));
      Serial.println(message);
      getSerialCommand(getValue(message, ' ', 0), getValue(message, ' ', 1));
      // Reset for the next message
      message_pos = 0;
    }
  }
}

void getSerialCommand(String cmd, String value)
{
  int val = value.toInt();
  Serial.print(F("CmdOut: "));
  if (cmd == "setWifi")
  {
    Serial.print(F("'setWifi' to "));
    if (val == 1)
    {
      globalControls.wifiSwitch = true;
      Serial.print(F(" 'ON' "));
    }
    else
    {
      globalControls.wifiSwitch = false;
      blinkCode = BLINK_WIFI_OFF;
      Serial.print(F(" 'OFF' "));
    }
  }
  else if (cmd == "resetToFactory")
  {
    Serial.print(F("'resetToFactory' to "));
    if (val == 1)
    {
      configManager.resetConfig();
      Serial.print(F(" reinitialize UserConfig data and reboot ... "));
      ESP.restart();
    }
  }
  else if (cmd == "rebootDevice")
  {
    Serial.print(F(" rebootDevice "));
    if (val == 1)
    {
      Serial.print(F(" ... rebooting ... "));
      ESP.restart();
    }
  }
  else
  {
    Serial.print(F("Cmd not recognized\n"));
  }
  Serial.print(F("\n"));
}

// main
// uint array for targetvalues with 10 steps
uint8_t dimTargetValues[10] = {0, 50, 1, 60, 3, 70, 5, 80, 7, 90};
uint8_t dimCounter = 0;
void loop()
{
  unsigned long currentMillis = millis();
  // skip all tasks if update is running
  if (updateInfo.updateState != UPDATE_STATE_IDLE)
  {
    if (updateInfo.updateState == UPDATE_STATE_PREPARE)
    {
      updateInfo.updateState = UPDATE_STATE_INSTALLING;
    }
    if (updateInfo.updateState == UPDATE_STATE_DONE)
    {
      updateInfo.updateState = UPDATE_STATE_RESTART;
    }
    return;
  }
  // check for wifi networks scan results
  scanNetworksResult();

#if defined(ESP8266)
  // serving domain name
  MDNS.update();
#endif

  // runner for mqttClient to hold a already etablished connection
  if (userConfig.mqttActive && WiFi.status() == WL_CONNECTED)
    mqttHandler.loop();

  // 1ms task
  if (currentMillis - previousMillis1ms >= interval1ms)
  {
    previousMillis1ms = currentMillis;
    // -------->
    for (int i = 0; i < LED_DIMMER_COUNT; i++)
    {
      dimmerLedArray[i].loop();
    }
  }

  // 10ms task
  if (currentMillis - previousMillis10ms >= interval10ms)
  {
    previousMillis10ms = currentMillis;
    // -------->
  }

  // 50ms task
  if (currentMillis - previousMillis50ms >= interval50ms)
  {
    previousMillis50ms = currentMillis;
    // -------->
  }

  // 100ms task
  if (currentMillis - previousMillis100ms >= interval100ms)
  {
    previousMillis100ms = currentMillis;
    // -------->
    // led blink code only 5 min after startup
    if ((platformData.currentNTPtime - platformData.dtuGWstarttime) < 300)
      blinkCodeTask();
    serialInputTask();
    // get current LED values
    ledDimmerStruct currentDimValues[5];
    for (int i = 0; i < LED_DIMMER_COUNT; i++)
      currentDimValues[i] = dimmerLedArray[i].getDimmerValues();

    // transfer data to webserver
    dtuWebServer.setLEDdata(currentDimValues);

    if (userConfig.mqttActive)
    {
      // getting dimTargetValue over MQTT, only on demand
      for (int i = 0; i < LED_DIMMER_COUNT; i++)
      {
        LedDimmerSet lastSetting = mqttHandler.getLedDimmerSet(i);
        bool gotChanges = false;

        if (lastSetting.setValueUpdate)
        {
          Serial.println("MQTT-LED: changed led " + String(i) + " dimmer value to '" + String(lastSetting.setValue) + "'");
          dimmerLedArray[i].setDimValue(lastSetting.setValue);
          gotChanges = true;
        }
        if (lastSetting.setSwitchUpdate)
        {
          Serial.println("MQTT_LED: changed led " + String(i) + " switch to '" + String(lastSetting.setSwitch) + "'");
          if (lastSetting.setSwitch && currentDimValues[i].dimValueTarget == 0)
            lastSetting.setValue = 100;
          else if (!lastSetting.setSwitch && currentDimValues[i].dimValueTarget > 0)
            lastSetting.setValue = 0;

          dimmerLedArray[i].setDimValue(lastSetting.setValue);
          gotChanges = true;
        }
        if (lastSetting.setdimValueStepUpdate)
        {
          Serial.println("MQTT_LED: changed led " + String(i) + " dimValueStep to '" + String(lastSetting.setdimValueStep) + "'");
          userConfig.ledDimmerConfigs[i].dimValueStep = lastSetting.setdimValueStep;
          gotChanges = true;
        }
        if (lastSetting.setdimValueStepDelayUpdate)
        {
          Serial.println("MQTT_LED: changed led " + String(i) + " dimValueStepDelay to '" + String(lastSetting.setdimValueStepDelay) + "'");
          userConfig.ledDimmerConfigs[i].dimValueStepDelay = lastSetting.setdimValueStepDelay;
          gotChanges = true;
        }
        if (gotChanges)
        {
          setConfigValuesToDimmer();
          ledDimmerStruct currentDimValues = dimmerLedArray[i].getDimmerValues();
          if (currentDimValues.ledPWMpin != 255)
            updateValuesToMqttLED(currentDimValues, "led" + String(i), true);
        }
      }

      updateValuesToMqtt();
    }

    platformData.currentNTPtime = timeClient.getEpochTime() < (12 * 60 * 60) ? (12 * 60 * 60) : timeClient.getEpochTime();
    platformData.currentNTPtimeFormatted = timeClient.getFormattedTime();
  }

  // short task
  if (currentMillis - previousMillisShort >= intervalShort)
  {
    // Serial.printf("\n>>>>> %02is task - state --> ", int(intervalShort));
    // Serial.print("local: " + getTimeStringByTimestamp(dtuGlobalData.currentTimestamp));
    // Serial.print(" --- NTP: " + timeClient.getFormattedTime() + " --- currentMillis " + String(currentMillis) + " --- ");
    previousMillisShort = currentMillis;
    // Serial.print(F("free mem: "));
    // Serial.print(ESP.getFreeHeap());
    // Serial.print(F(" - heap fragm: "));
    // Serial.print(ESP.getHeapFragmentation());
    // Serial.print(F(" - max free block size: "));
    // Serial.print(ESP.getMaxFreeBlockSize());
    // Serial.print(F(" - free cont stack: "));
    // Serial.print(ESP.getFreeContStack());
    // Serial.print(F(" \n"));
    // -------->

    if (!userConfig.wifiAPstart)
    {
      if (globalControls.wifiSwitch)
        checkWifiTask();
      else
      {
        // stopping connection to DTU before go wifi offline
        WiFi.disconnect();
      }
    }

    // if (updateInfo.updateInfoRequested)
    // {
    //   getUpdateInfo();
    // }

    // update dimmer values from user config
    setConfigValuesToDimmer();
  }

  // 5s task
  if (currentMillis - previousMillis5000ms >= interval5000ms)
  {
    Serial.printf(">>>>> %02is task - state --> ", int(interval5000ms));
    if (userConfig.wifiAPstart)
      Serial.print(" --- in AP mode for first config --- ");
    Serial.print(" --- NTP: " + timeClient.getFormattedTime() + " - " + getTimeStringByTimestamp(platformData.currentNTPtime) + " --- " + String(platformData.currentNTPtime));

    // get current LED values
    ledDimmerStruct currentDimValues[5];
    for (int i = 0; i < LED_DIMMER_COUNT; i++)
      currentDimValues[i] = dimmerLedArray[i].getDimmerValues();

    Serial.print(" - [pin] dim | tgt => ");
    for (int i = 0; i < LED_DIMMER_COUNT; i++)
    {
      if (currentDimValues[i].ledPWMpin != 255)
      {
        char formattedDimValue[4];
        sprintf(formattedDimValue, "%3d", currentDimValues[i].dimValue);
        char formattedTargetValue[4];
        sprintf(formattedTargetValue, "%3d", currentDimValues[i].dimValueTarget);

        Serial.print("  [" + String(currentDimValues[i].ledPWMpin) + "] " + String(formattedDimValue) + " | " + String(formattedTargetValue) + " | " + String(currentDimValues[i].mainSwitch ? " ON" : "OFF"));
      }
    }
    Serial.println("");

    previousMillis5000ms = currentMillis;
    // -------->
    // dimmerLed_0.setDimValue(dimTargetValues[dimCounter]);
    // dimmerLed_1.setDimValue(dimTargetValues[dimCounter]);
    // dimmerLed_2.setDimValue(dimTargetValues[dimCounter]);
    // dimmerLed_3.setDimValue(dimTargetValues[dimCounter]);
    // dimmerLed_4.setDimValue(dimTargetValues[dimCounter]);

    // dimCounter++;
    // if (dimCounter > 10)
    // {
    //   dimCounter = 0;
    // }

    // -----------------------------------------
    if (WiFi.status() == WL_CONNECTED)
    {
      // get current RSSI to AP
      int wifiPercent = 2 * (WiFi.RSSI() + 100);
      if (wifiPercent > 100)
        wifiPercent = 100;
      platformData.wifiRSSI = wifiPercent;
      // Serial.print(" --- RSSI to AP: '" + String(WiFi.SSID()) + "': " + String(dtuGlobalData.wifi_rssi_gateway) + " %");
    }
  }

  // mid task
  if (currentMillis - previousMillis30000ms >= interval30000ms)
  {
    Serial.printf(">>>>> %02is task - state --> ", int(interval30000ms));
    // Serial.print("local: " + getTimeStringByTimestamp(dtuGlobalData.currentTimestamp));
    Serial.println(" --- NTP: " + timeClient.getFormattedTime());

    previousMillis30000ms = currentMillis;
    // -------->
  }

  // long task
  if (currentMillis - previousMillisLong >= intervalLong)
  {
    // Serial.printf("\n>>>>> %02is task - state --> ", int(interval5000ms));
    // Serial.print("local: " + getTimeStringByTimestamp(dtuGlobalData.currentTimestamp));
    // Serial.print(" --- NTP: " + timeClient.getFormattedTime() + " --- currentMillis " + String(currentMillis) + " --- ");

    previousMillisLong = currentMillis;
    // -------->
    if (WiFi.status() == WL_CONNECTED)
    {
      timeClient.update();
    }
  }
}