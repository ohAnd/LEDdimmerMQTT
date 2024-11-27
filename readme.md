# LEDdimmerMQTT

## Contents
- [LEDdimmerMQTT](#leddimmermqtt)
  - [Contents](#contents)
  - [idea](#idea)
  - [features](#features)
  - [api](#api)
    - [data - http://\<ip\_to\_your\_device\>/api/data.json](#data---httpip_to_your_deviceapidatajson)
    - [info - http://\<ip\_to\_your\_device\>/api/info.json](#info---httpip_to_your_deviceapiinfojson)
  - [MQTT integration/ configuration](#mqtt-integration-configuration)
  - [installation](#installation)
    - [hardware](#hardware)
    - [first installation to the ESP device](#first-installation-to-the-esp-device)
      - [example for ESP8266](#example-for-esp8266)
      - [example for ESP32](#example-for-esp32)
      - [first setup with access point](#first-setup-with-access-point)
      - [return to factory mode](#return-to-factory-mode)
    - [update](#update)
  - [releases](#releases)
    - [main](#main)
    - [snapshot](#snapshot)
  - [troubleshooting](#troubleshooting)
  - [build environment](#build-environment)
    - [platformio](#platformio)
    - [hints for workflow](#hints-for-workflow)


## idea

- Easy integration of a PWM driven LED driver
- after main setup it should be seamlessly integratable in existing smart home system e.g. HomeAssistant or openHAB via MQTT Autodiscovery/ AutoConfig
- Extendable to more direct LED controls (currently limited to 5 pins/ PWM controlable driver)

## features

- LED settings
  - steps = resolution of transitions at changing to a new target dimming value
  - stepDelay = time in milli seconds to wait per stepsize
  - used PIN (not usable PINs: ledPWMpin == 255 || ledPWMpin <= 1 || ledPWMpin == 3 || (ledPWMpin >= 6 && ledPWMpin <= 11) || ledPWMpin == 24 || ledPWMpin >= 34)
- serving the read data per /api/data.json
- serving own access point in factory mode for first setup
- web application will be directly served by the system
- settings of needed user data over the web app (stored in a json-file in local flash file system - extensions of user setup will not lead to breakable changes)
  - select found local wifi and enter/ save the needed wifi password
  - configurable data for MQTT settings incl. HomeAssistant AutoDiscovery
  - advanced web config for all config parameter (http://IP_domain/config) - expert mode
- manual OTA/ web Update via web ui (hint: only stable if the wifi connection is above ~ 50%)

## api

### data - http://<ip_to_your_device>/api/data.json

<details>
<summary>expand to see json example</summary>

```json 
{
  "ntpStamp": 1729431663,
  "starttime": 1729431519,
  "leds": [
    {
      "mainSwitch": 0,
      "dimValue": 0,
      "dimValueTarget": 0,
      "dimValueRaw": 0,
      "dimValueStep": 1,
      "dimValueStepDelay": 5
    },
    {
      "mainSwitch": 0,
      "dimValue": 0,
      "dimValueTarget": 0,
      "dimValueRaw": 0,
      "dimValueStep": 1,
      "dimValueStepDelay": 4
    }
  ]
}
```
</details>

### info - http://<ip_to_your_device>/api/info.json

<details>
<summary>expand to see json example</summary>

```json 
{
  "chipid": 12345678,
  "chipType": "ESP32",
  "host": "LEDdimmerMQTT_12345678",
  "initMode": 0,
  "firmware": {
    "version": "0.0.2",
    "versiondate": "08.10.2024 - 09:19:48",
    "versionServer": "checking",
    "versiondateServer": "...",
    "versionServerRelease": "checking",
    "versiondateServerRelease": "...",
    "selectedUpdateChannel": "0",
    "updateAvailable": 0
  },
 "ledSettings": [
    {
      "ledPWMpin": 4,
      "dimValueStep": 1,
      "dimValueStepDelay": 5
    },
    {
      "ledPWMpin": 5,
      "dimValueStep": 1,
      "dimValueStepDelay": 4
    }
  ],
  "mqttConnection": {
    "mqttActive": 1,
    "mqttIp": "homeassistant",
    "mqttPort": 1883,
    "mqttUseTLS": 0,
    "mqttUser": "user",
    "mqttPass": "pass",
    "mqttMainTopic": "LEDdimmerMQTT_12345678",
    "mqttHAautoDiscoveryON": 1
  },
  "wifiConnection": {
    "wifiSsid": "privateWifi",
    "wifiPassword": "privateWifiPass",
    "rssiGW": 80,
    "wifiScanIsRunning": 0,
    "networkCount": 0,
    "foundNetworks": [
      {
        "name": "Name1 Wlan",
        "wifi": 62,
        "rssi": -69,
        "chan": 1
      },
      {
        "name": "name2-wifi",
        "wifi": 48,
        "rssi": -76,
        "chan": 3
      }
    ]
  }
}
```
</details>

## MQTT integration/ configuration

- set the IP to your MQTT broker
- set the MQTT user and MQTT password
- set the main topic e.g. 'LEDdimmerMQTT_12345678' for the pubished data (default: is `LEDdimmerMQTT_<ESP chip id>` and has to be unique in your environment)

- Home Assistant Auto Discovery
  - you can set HomeAssistant Auto Discovery, if you want to auto configure the LEDdimmerMQTT for your HA installation 
  - switch to ON means - with every restart/ reconnection of the LEDdimmerMQTT the so called config messages will be published for HA and HA will configure (or update) all the given entities of LEDdimmerMQTT incl. the set value for PowerLimit
  - switch to OFF means - all the config messages will be deleted and therefore the LEDdimmerMQTT will be removed from HA (base publishing of data will be remain the same, if MQTT is activated)
  - detail note:
    - if you use the default main topic e.g. `LEDdimmerMQTT_<ESP chip id>` then config and state topic will be placed at the same standard HA auto discovery path, e.g.
      - config: `homeassistant/light/LEDdimmerMQTT_12345678/led0/config`
      - state: `homeassistant/light/LEDdimmerMQTT_12345678/led0/dimmer/state`
      - set: `homeassistant/light/LEDdimmerMQTT_12345678/led0/dimmer/set`

## installation
### hardware
- ESP8266/ ESP32/ ESP32-S2 (LOLIN_S2_MINI) based boards
- GPIO 4 for PWM out to PWM driven DC regulator (e.g. https://www.amazon.de/dp/B0CBK7D1GD?ref=ppx_yo2ov_dt_b_fed_asin_title)

### first installation to the ESP device
#### example for ESP8266
1. download the preferred release as binary (see below)
2. [only once] flash the esp8266 board with the [esp download tool](https://www.espressif.com/en/support/download/other-tools)
   1. choose bin file at address 0x0
   2. SPI speed 40 MHz
   3. SPI Mode QIO
   4. select your COM port and baudrate = 921600
   5. press start ;-)
3. all further updates are done by [OTA](###-regarding-base-framework) or [webupdate](###-update)

You can also use the esptool.py as described shortly here https://github.com/ohAnd/dtuGateway/discussions/46#discussion-7106516 by @netzbasteln

#### example for ESP32
see also https://github.com/ohAnd/dtuGateway/discussions/35#discussioncomment-10519821
1. download the preferred release as binary (see below)
2. [only once] flash the esp32 board with the [esp download tool](https://www.espressif.com/en/support/download/other-tools)
   1. get the needed bin files (see at doc/esp32_factoryFlash)
      1. [bootloader.bin](doc/esp32_factoryFlash/bootloader.bin) or pick ESP32_S2 files
      2. [partions.bin](doc/esp32_factoryFlash/partitions.bin)  or pick ESP32_S2 files
      3. [boot_app0.bin](doc/esp32_factoryFlash/boot_app0.bin) or pick ESP32_S2 files
      4. current [release] or [snapshot]
   2. select inside the flash tool the files 1.1 - 1.4 and set the following start adresses
      1. bootloader.bin => 0x1000
      2. partionions.bin => 0x8000
      3. boot_app0.bin => 0xE000
      4. firmware => 0x10000
   3. SPI speed 40 MHz
   5. SPI Mode QIO
   6. select your COM port and baudrate = 921600
   8. press start ;-)
3. all further updates are done by [OTA](###-regarding-base-framework) or [webupdate](###-update)

#### first setup with access point

1. connect with the AP LEDdimmerMQTT_<chipID> (on smartphone sometimes you have to accept the connection explicitly with the knowledge there is no internet connectivity)
2. open the website http://192.168.4.1 for the first configuration
3. choose your wifi
4. type in the wifi password - save
5. in webfrontend setting your MQTT data -> set the IP and port (e.g. 192.178.0.42:1883) of your mqtt broker and the user and passwort that your have for this instance

#### return to factory mode
1. connect your ESP with serial (115200 baud) in a COM terminal
2. check if you receive some debug data from the device
3. type in `resetToFactory 1`
4. response of the device will be `reinitialize UserConfig data and reboot ...`
5. after reboot the device starting again in AP mode for first setup

### update
Via the web ui you can select the firmware file and start the update process. Please use the right firmware file according to your processor ESP8266 / ESP32 / ESP32-S2.

## releases
### main
latest release - changes will documented by commit messages
https://github.com/ohAnd/LEDdimmerMQTT/releases/latest

(to be fair, the amount of downloads is the count of requests from the client to check for new firmware for the OTA update)

![GitHub Downloads (all assets, latest release)](https://img.shields.io/github/downloads/ohand/LEDdimmerMQTT/latest/total)
![GitHub (Pre-)Release Date](https://img.shields.io/github/release-date/ohand/LEDdimmerMQTT)
![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/ohand/LEDdimmerMQTT/main_build.yml)

### snapshot
snapshot with latest build
https://github.com/ohAnd/LEDdimmerMQTT/releases/tag/snapshot

![GitHub Downloads (all assets, specific tag)](https://img.shields.io/github/downloads/ohand/LEDdimmerMQTT/snapshot/total)
![GitHub (Pre-)Release Date](https://img.shields.io/github/release-date-pre/ohand/LEDdimmerMQTT)
![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/ohand/LEDdimmerMQTT/dev_build.yml)

## troubleshooting

- if the config file is corrupted due to whatever reason with unexpected behavior - connect with serial terminal and type in the command `resetToFactory 1` - the config file be rewritten with the default values
- if in the first startup mode a wrong ssid/ password was entered, then also `resetToFactory 1` 

## build environment

fully covered with github actions

building on push to develop and serving as a snapshot release with direct connection to the device - available updates will be locally checked and offered to the user for installation 

hint: 
> For automatic versioning there is a file called ../include/buildnumber.txt expected. With the content "localDev" or versionnumber e.g. "1.0.0" in first line. (File is blocked by .gitignore for GitHub actions to run.)



### platformio
- https://docs.platformio.org/en/latest/core/installation/methods/installer-script.html#local-download-macos-linux-windows

### hints for workflow
- creating dev release (https://blog.derlin.ch/how-to-create-nightly-releases-with-github-actions)
