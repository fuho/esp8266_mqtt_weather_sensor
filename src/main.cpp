#include <FS.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <WiFiManager.h>        // https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>        // https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>       // https://github.com/knolleary/pubsubclient

#include "main.h"

WiFiClient wifiClient;
PubSubClient pubSubClient(wifiClient);

char mqtt_server[40] = "test.mosquitto.org";
char mqtt_port[6] = "1883";

// id/name, placeholder/prompt, default, length
WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);

void configModeCallback(WiFiManager *myWiFiManager) {
    Serial.println("*WM: Entering config mode...");
    Serial.println(WiFi.softAPIP());
    Serial.println(myWiFiManager->getConfigPortalSSID());
}

void saveConfigCallback() {
    Serial.println("*WM: Saving config...");
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    strcpy(mqtt_port, custom_mqtt_port.getValue());
    Serial.println("*WM: MQTT Server: " + String(mqtt_server));
    Serial.println("*WM: MQTT Port: " + String(mqtt_port));
    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
        Serial.println("*FS: Failed to open config file for writing!");
    }
    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
}

std::function<void(char *, uint8_t *, unsigned int)> pubSubCallback(char *topic, byte *payload, unsigned int length) {
    Serial.print("*PubSub: Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++) {
        Serial.print((char) payload[i]);
    }
    Serial.println();

    // Switch on the LED if an 1 was received as first character
    if ((char) payload[0] == '1') {
        digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
        // but actually the LED is on; this is because
        // it is acive low on the ESP-01)
    } else {
        digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
    }
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

void initBlink() {
    pinMode(BUILTIN_LED, OUTPUT);
}

void initWM() {
    WiFiManager wifiManager;
    //wifiManager.resetSettings();
    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_port);
    wifiManager.setAPCallback(configModeCallback);
    wifiManager.setSaveConfigCallback(saveConfigCallback);
    if (!wifiManager.autoConnect()) {
        Serial.println("*WM: Failed to connect, restarting...");
        ESP.reset();
        delay(1000);
    };
    Serial.println("*WM: Connected!");
}

void initFS() {
    if (SPIFFS.begin()) {
        Serial.println("*FS: Mounted file system");
        if (SPIFFS.exists("/config.json")) {
            //file exists, reading and loading
            Serial.println("*FS: Reading config file");
            File configFile = SPIFFS.open("/config.json", "r");
            if (configFile) {
                Serial.println("*FS: Opened config file");
                size_t size = configFile.size();
                // Allocate a buffer to store contents of the file.
                std::unique_ptr<char[]> buf(new char[size]);

                configFile.readBytes(buf.get(), size);
                DynamicJsonBuffer jsonBuffer;
                JsonObject &json = jsonBuffer.parseObject(buf.get());
                json.printTo(Serial);
                if (json.success()) {
                    Serial.println("\n*FS: parsed json");

                    strcpy(mqtt_server, json["mqtt_server"]);
                    strcpy(mqtt_port, json["mqtt_port"]);

                } else {
                    Serial.println("*FS: Failed to load json config");
                }
            }
        }
    } else {
        Serial.println("*FS: Failed to mount FS");
    }
}

void initPubSub() {
    pubSubClient.setServer(mqtt_server, (uint16_t) atoi(mqtt_port));
    pubSubClient.setCallback(pubSubCallback);
}

void setup() {
    // Init serial
    Serial.begin(230400);
    Serial.println("Booting ESP8266");
    // Init FileSystem
    initFS();
    // Init wifi manager
    initWM();
    // Init Over The Air firmware upload
    Serial.println("*OTA: Initializing...");
    initOta();
    Serial.println("*OTA: Ready");
    Serial.print("*OTA: IP address: ");
    Serial.println(WiFi.localIP());
    // Init PubSub
    initPubSub();
    initBlink();
}

void reconnect() {
    Serial.print("*PubSub: Attempting MQTT connection...");
    // Attempt to connect
    if (pubSubClient.connect("ESP8266Client")) {
        Serial.println("*PubSub: connected");
        // Once connected, publish an announcement...
        pubSubClient.publish("outTopic", "hello world");
        // ... and resubscribe
        pubSubClient.subscribe("inTopic");
    } else {
        Serial.print("*PubSub: Failed!, rc=");
        Serial.print(pubSubClient.state());
    }
}

void blink() {
    static unsigned long lastToggle;
    if (millis() > lastToggle + 50) {
        lastToggle = millis();
        digitalWrite(BUILTIN_LED, (uint8_t) !digitalRead(BUILTIN_LED));
    }
}


void pubSubHandle() {
    static unsigned long lastMsg;
    static unsigned long value;
    static char msg[50];
    if (!pubSubClient.connected()) {
        reconnect();
    }
    pubSubClient.loop();
    unsigned long now = millis();
    if (now > lastMsg + 1000) {
        lastMsg = now;
        ++value;
        snprintf(msg, 75, "hello world #%ld", value);
        Serial.print("Publish message: ");
        Serial.println(msg);
        pubSubClient.publish("outTopic", msg);
    }
}

void loop() {
    ArduinoOTA.handle();
    blink();
    pubSubHandle();
}


