#include "main.h"
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <WiFiManager.h>

void configModeCallback (WiFiManager * myWiFiManager){
    Serial.println("*WiFi Manager: Entering config mode...");
    Serial.println(WiFi.softAPIP());
    Serial.println(myWiFiManager->getConfigPortalSSID());
}

void initOta() {
    // Init ArduinoOta
    ArduinoOTA.onStart([]() {
        Serial.println("*OTA: Start");
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\n*OTA: End");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("*OTA: Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("*OTA: Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("*OTA: Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("*OTA: Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("*OTA: Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("*OTA: Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("*OTA: End Failed");
    });
    ArduinoOTA.begin();
}

void initBlink(){
    pinMode(BUILTIN_LED, OUTPUT);
}

void blink() {
    static unsigned long lastToggle;
    if(millis() > lastToggle + 50 ){
        lastToggle =millis();
        digitalWrite(BUILTIN_LED, (uint8_t) ! digitalRead(BUILTIN_LED));
    }
}

void setup() {
    // Init serial
    Serial.begin(230400);
    Serial.println("Booting ESP8266");
    // Init wifi manager
    WiFiManager wifiManager;
    wifiManager.resetSettings();
    wifiManager.setAPCallback(configModeCallback);
    if(! wifiManager.autoConnect)){
        Serial.println("*WM: Failed to connect, restarting...");
        ESP.reset();
        delay(1000);

    };
    Serial.println("*WM: Connected!");
    Serial.println("*OTA: Initializing...");
    initOta();
    Serial.println("*OTA: Ready");
    Serial.print("*OTA: IP address: ");
    Serial.println(WiFi.localIP());
    initBlink();
}

void loop() {
    ArduinoOTA.handle();
    blink();
    delay(50);
}


