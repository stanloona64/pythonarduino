#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <ArduinoJson.h>
#include <NimBLEUtils.h>
#include <NimBLEServer.h>
#include <NimBLEDevice.h>
#include <NimBLEAdvertising.h>
#include <WebServer.h>
const char compileDate[] = __DATE__ " " __TIME__;
char apName[] = "ESP32-xxxxxxxxxxxx";
bool usePrimAP = true;
bool hasCredentials = false;
volatile bool isConnected = false;
bool connStatusChanged = false;

WebServer server(80);
String receivedData = "";

void handleRoot() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "Ready");
}

void handleGet() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  Serial.println("get success");
  String data = server.arg("data");
  Serial.println("Data (GET): " + data);
  server.send(200, "text/plain", "Data Received");
}

void handlePost() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  String data = server.arg("data");
  Serial.println("Data (Post): " + data);
  server.send(200, "text/plain", "Data Received");
}

void handleUpload() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    // Serial.println("Receiving data:");
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    Serial.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    server.send(200, "text/plain", "Data: ");
  }
}

void createName()
{
    uint8_t baseMac[6];
    esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
    sprintf(apName, "ESP32-%02X%02X%02X%02X%02X%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
}

#define SERVICE_UUID "0000AAAA-EAD2-11E7-80C1-9A214CF093AE"
#define WIFI_UUID "00005555-EAD2-11E7-80C1-9A214CF093AE"

String ssidPrim;
String ssidSec;
String pwPrim;
String pwSec;
String valueBT;
String ip = "";

BLECharacteristic *pCharacteristicWiFi;
BLEAdvertising *pAdvertising;
BLEService *pService;
BLEServer *pServer;

StaticJsonDocument<200> jsonBuffer;

class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer)
    {
        Serial.println("BLE client connected");
    };

    void onDisconnect(BLEServer *pServer)
    {
        Serial.println("BLE client disconnected");
        pAdvertising->start();
    }
};

class MyCallbackHandler : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        std::string value = pCharacteristic->getValue();
        if (value.length() == 0)
        {
            return;
        }
        valueBT = String((char *)&value[0]);
        Serial.println("Received over BLE: " + String((char *)&value[0]));

        auto error = deserializeJson(jsonBuffer, (char *)&value[0]);

        if (error)
        {
            Serial.println("Received invalid JSON");
        }
        else
        {
            if (jsonBuffer.containsKey("ssidPrim") &&
                jsonBuffer.containsKey("pwPrim") &&
                jsonBuffer.containsKey("ssidSec") &&
                jsonBuffer.containsKey("pwSec"))
            {
                ssidPrim = jsonBuffer["ssidPrim"].as<String>();
                pwPrim = jsonBuffer["pwPrim"].as<String>();
                ssidSec = jsonBuffer["ssidSec"].as<String>();
                pwSec = jsonBuffer["pwSec"].as<String>();

                Preferences preferences;
                preferences.begin("WiFiCred", false);
                preferences.putString("ssidPrim", ssidPrim);
                preferences.putString("ssidSec", ssidSec);
                preferences.putString("pwPrim", pwPrim);
                preferences.putString("pwSec", pwSec);
                preferences.putBool("valid", true);
                preferences.end();

                Serial.println("Received over bluetooth:");
                Serial.println("primary SSID: " + ssidPrim + " password: " + pwPrim);
                Serial.println("secondary SSID: " + ssidSec + " password: " + pwSec);
                connStatusChanged = true;
                hasCredentials = true;
            }
            else if (jsonBuffer.containsKey("erase"))
            {
                Serial.println("Received erase command");
                Preferences preferences;
                preferences.begin("WiFiCred", false);
                preferences.clear();
                preferences.end();
                connStatusChanged = true;
                hasCredentials = false;
                ssidPrim = "";
                pwPrim = "";
                ssidSec = "";
                pwSec = "";

                int err;
                err = nvs_flash_init();
                Serial.println("nvs_flash_init: " + err);
                err = nvs_flash_erase();
                Serial.println("nvs_flash_erase: " + err);
            }
            else if (jsonBuffer.containsKey("reset"))
            {
                WiFi.disconnect();
                esp_restart();
            }
        }
        jsonBuffer.clear();
    };
    
    void gotIP(WiFiEvent_t event, WiFiEventInfo_t info)
    {
    isConnected = true;
    connStatusChanged = true;
    ip = WiFi.localIP().toString();
    }

    void onRead(BLECharacteristic *pCharacteristic)
    {
        Serial.println("BLE onRead request");
        String wifiCredentials;

        jsonBuffer.clear();
        jsonBuffer["ssidPrim"] = ssidPrim;
        jsonBuffer["pwPrim"] = pwPrim;
        jsonBuffer["ssidSec"] = ssidSec;
        jsonBuffer["pwSec"] = pwSec;
        jsonBuffer["ip"] = ip; // Add IP address to the JSON
        serializeJson(jsonBuffer, wifiCredentials);

        int keyIndex = 0;
        Serial.println("Stored settings: " + wifiCredentials);
        for (int index = 0; index < wifiCredentials.length(); index++)
        {
            wifiCredentials[index] = (char)wifiCredentials[index] ^ (char)apName[keyIndex];
            keyIndex++;
            if (keyIndex >= strlen(apName))
                keyIndex = 0;
        }
        pCharacteristicWiFi->setValue((uint8_t *)&ip[0], ip.length());
        jsonBuffer.clear();
    }
};

void initBLE()
{
    NimBLEDevice::init(apName);
    NimBLEDevice::setPower(ESP_PWR_LVL_P7);

    Serial.printf("BLE advertising using %s\n", apName);
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityPasskey(123456);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    pService = pServer->createService(SERVICE_UUID);
    pCharacteristicWiFi = pService->createCharacteristic(
        WIFI_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC);
    pCharacteristicWiFi->setCallbacks(new MyCallbackHandler());
    pService->start();
    pAdvertising = pServer->getAdvertising();
    pAdvertising->addServiceUUID(WIFI_UUID);
    pAdvertising->start();
}

void gotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
    isConnected = true;
    connStatusChanged = true;
    ip = WiFi.localIP().toString();
}

void lostCon(WiFiEvent_t event, WiFiEventInfo_t info)
{
    isConnected = false;
    connStatusChanged = true;
    ip = "";
}

bool scanWiFi()
{
    int8_t rssiPrim = -100;
    int8_t rssiSec = -100;
    bool result = false;

    Serial.println("Start scanning for networks");

    WiFi.disconnect(true);
    WiFi.enableSTA(true);
    WiFi.mode(WIFI_STA);

    int apNum = WiFi.scanNetworks(false, true, false, 1000);
    if (apNum == 0)
    {
        Serial.println("Found no networks?????");
        return false;
    }

    byte foundAP = 0;
    bool foundPrim = false;

    for (int index = 0; index < apNum; index++)
    {
        String ssid = WiFi.SSID(index);
        Serial.println("Found AP: " + ssid + " RSSI: " + WiFi.RSSI(index));
        if (!strcmp((const char *)&ssid[0], (const char *)&ssidPrim[0]))
        {
            Serial.println("Found primary AP");
            foundAP++;
            foundPrim = true;
            rssiPrim = WiFi.RSSI(index);
        }
        if (!strcmp((const char *)&ssid[0], (const char *)&ssidSec[0]))
        {
            Serial.println("Found secondary AP");
            foundAP++;
            rssiSec = WiFi.RSSI(index);
        }
    }

    switch (foundAP)
    {
    case 0:
        result = false;
        break;
    case 1:
        if (foundPrim)
        {
            usePrimAP = true;
        }
        else
        {
            usePrimAP = false;
        }
        result = true;
        break;
    default:
        Serial.printf("RSSI Prim: %d Sec: %d\n", rssiPrim, rssiSec);
        if (rssiPrim > rssiSec)
        {
            usePrimAP = true;
        }
        else
        {
            usePrimAP = false;
        }
        result = true;
        break;
    }
    return result;
}

void connectWiFi()
{
    WiFi.onEvent(gotIP, ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent(lostCon, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    WiFi.disconnect(true);
    WiFi.enableSTA(true);
    WiFi.mode(WIFI_STA);

    Serial.println();
    Serial.print("Start connection to ");
    if (usePrimAP)
    {
        Serial.println(ssidPrim);
        WiFi.begin(ssidPrim.c_str(), pwPrim.c_str());
    }
    else
    {
        Serial.println(ssidSec);
        WiFi.begin(ssidSec.c_str(), pwSec.c_str());
    }
}

void initBLEWIFI()
{
    createName();
    Serial.print("Build: ");
    Serial.println(compileDate);

    Preferences preferences;
    preferences.begin("WiFiCred", false);
    bool hasPref = preferences.getBool("valid", false);
    if (hasPref)
    {
        ssidPrim = preferences.getString("ssidPrim", "");
        ssidSec = preferences.getString("ssidSec", "");
        pwPrim = preferences.getString("pwPrim", "");
        pwSec = preferences.getString("pwSec", "");

        if (ssidPrim.equals("") || pwPrim.equals("") || ssidSec.equals("") || pwPrim.equals(""))
        {
            Serial.println("Found preferences but credentials are invalid");
        }
        else
        {
            Serial.println("Read from preferences:");
            Serial.println("primary SSID: " + ssidPrim + " password: " + pwPrim);
            Serial.println("secondary SSID: " + ssidSec + " password: " + pwSec);
            hasCredentials = true;
        }
    }
    else
    {
        Serial.println("Could not find preferences, need send data over BLE");
    }
    preferences.end();

    initBLE();

    if (hasCredentials)
    {
        if (!scanWiFi())
        {
            Serial.println("Could not find any AP");
        }
        else
        {
            connectWiFi();
        }
    }
    Serial.println("\n\n##################################");
    Serial.printf("Internal Total heap %d, internal Free Heap %d\n", ESP.getHeapSize(), ESP.getFreeHeap());
    Serial.printf("SPIRam Total heap %d, SPIRam Free Heap %d\n", ESP.getPsramSize(), ESP.getFreePsram());
    Serial.printf("ChipRevision %d, Cpu Freq %d, SDK Version %s\n", ESP.getChipRevision(), ESP.getCpuFreqMHz(), ESP.getSdkVersion());
    Serial.printf("Flash Size %d, Flash Speed %d\n", ESP.getFlashChipSize(), ESP.getFlashChipSpeed());
    Serial.println("##################################\n\n");
}