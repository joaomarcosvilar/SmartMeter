#ifndef INTERFACELIB_H
#define INTERFACELIB_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>

#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

#include "LoRaMESH.h"
#include "certificates.h"

class Interface
{
public:
    Interface();
    void begin(JsonDocument data);
    void setup();
    bool send(const char *info);

private:
    void initializeMQTT();
    JsonDocument data;
    bool MeshSend(const char *info);
    bool WiFiSend(const char *info);
    String selecInter;
    bool debug = false;
};
#endif

#define TimeOut 10000 // em milisegundos
#define AWS_IOT_PUBLISH_TOPIC "smartmeter/power"

WiFiClientSecure espClient = WiFiClientSecure();
PubSubClient MQTTclient(espClient);

LoRaEnd lora(17, 16);

Interface::Interface() {}

void Interface::begin(JsonDocument _data)
{
    data.set(_data);
    debug = data["debug"];

    // Inicializa o LoRaMesh
    if (data["LoRaMESH"]["Status"])
    {
        lora.begin(data);
        selecInter = "LoRaMESH";
    }

    // Inicializa o WiFi e conecta com o broker MQTT
    if (data["WiFi"]["Status"])
    {
        WiFi.begin();
        // Inicialização do WiFi
        const char *ssid = data["WiFi"]["SSID"];
        const char *password = data["WiFi"]["Password"];

        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED)
        {
            Serial.print('.');
            vTaskDelay(pdMS_TO_TICKS(500));
            if ((millis() - start) > TimeOut)
            {
                Serial.println("WiFi error, initializing...");
                ESP.restart();
            }
        }
        if (debug)
            Serial.println(WiFi.localIP());
        selecInter = "WiFi";
        initializeMQTT();
        return;
    }

    // Inicializa protocolo GSM
    // if (data["PPP"]["Status"]){selecInter= "PPP";
    //}

    if ((data["WiFi"]["Status"]) || (data["PPP"]["Status"]))
    {
    }
}

void Interface::initializeMQTT()
{
    if (debug)
        Serial.println("Initialized MQTT conection.");
    // Conexão com o Broker
    serializeJson(data, Serial);
    const char *AWS_IOT_ENDPOINT = data["WiFi"]["AWS_IOT_ENDPOINT"];
    // Serial.println(AWS_IOT_ENDPOINT);
    const char *THINGNAME = data["WiFi"]["THINGNAME"];
    // Serial.println(THINGNAME);

    const char AWS_IOT_SUBSCRIBE_TOPIC[] = "smartmeter/subpower";
    // Configure WiFiClientSecure to use the AWS IoT device credentials
    espClient.setCACert(AWS_CERT_CA);
    espClient.setCertificate(AWS_CERT_CRT);
    espClient.setPrivateKey(AWS_CERT_PRIVATE);

    // Connect to the MQTT broker on the AWS endpoint we defined earlier
    Serial.print("setServer");
    MQTTclient.setServer(AWS_IOT_ENDPOINT, 8883);

    // Create a message handler
    // client.setCallback(messageHandler);

    if (debug)
        Serial.println("Connecting to AWS IOT");

    while (!MQTTclient.connect(THINGNAME))
    {
        if (debug)
        {
            Serial.print(MQTTclient.state());
            Serial.print(".");
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Subscribe to a topic
    MQTTclient.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);

    if (debug)
        Serial.println("AWS IoT Connected!");
}

bool Interface::send(const char *info)
{
    if (selecInter.equals("LoRaMESH"))
        return MeshSend(info);

    if (selecInter.equals("WiFi"))
        return WiFiSend(info);

    return false;

    // if (interface.equals("PPP"))
}

bool Interface::MeshSend(const char *info)
{
    String str = String(info);
    if (lora.sendMaster(str))
    {
        if (debug)
        {
            Serial.println(str);
            Serial.println("LoraMesh Send.");
        }
        return true;
    }
    else
        return false;
}

bool Interface::WiFiSend(const char *info)
{
    if (!MQTTclient.connected())
    {
        if (debug)
            Serial.println("Não conectado ao MQTT, reiniciando conexão");
        initializeMQTT();
    }
    Serial.println(String(info));
    if (MQTTclient.publish(AWS_IOT_PUBLISH_TOPIC, info))
    {
        MQTTclient.loop();
        if (debug)
        {
            Serial.println(info);
            Serial.println("Enviado.");
        }
        return true;
    }
    else
        return false;
}