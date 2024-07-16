#include <Arduino.h>
#include <bt.h>
#include <SPI.h>

void setup()
{
    Serial.begin(9600);
    initBLEWIFI();

    delay(500);
    while (!Serial) { ; }  // wait for serial port to connect. Needed for native USB port only
    Serial.println("Initializing SD card...");
    if (!SD.begin(CS)) {
        Serial.println("initialization failed!");
        return;
    }
    
    WriteFile(file_c);

    /*for (int i = 0; i < 4; i++) {
    ReadFile(files[i]);
    }*/


    Serial.println("initialization done.");
    server.on("/", handleRoot);
    server.on("/get", HTTP_GET, handleGet);
    server.on("/post", HTTP_POST, handlePost);
    server.on("/upload", HTTP_POST, handleUpload);
    server.begin();
    Serial.println("HTTP server started");
    handlePost();
}

void loop()
{
    server.handleClient();
    if (connStatusChanged)
    {        
        if (isConnected)
        {
            Serial.print("Connected to AP: ");
            Serial.print(WiFi.SSID());
            Serial.print(" with IP: ");
            Serial.print(WiFi.localIP());
            Serial.print(" RSSI: ");
            Serial.println(WiFi.RSSI());
        }
        else
        {
            if (hasCredentials)
            {
                Serial.println("Lost WiFi connection");
                if (!scanWiFi())
                {
                    Serial.println("Could not find any AP");
                }
                else
                {
                    Serial.println("Connected");
                    delay(1000);
                    connectWiFi();
                }
            }
        }
        connStatusChanged = false;
    }
}
