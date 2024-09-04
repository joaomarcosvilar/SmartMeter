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
    JsonDocument datas;
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
    datas.set(_data);
    bool debug = datas["debug"];

    // Inicializa o LoRaMesh
    if (datas["LoRaMESH"]["Status"])
    {
        // lora.begin(data["LoRaMESH"]); // ERROR
        selecInter = "LoRaMESH";
    }

    // Inicializa o WiFi e conecta com o broker MQTT
    if (datas["WiFi"]["Status"])
    {
        WiFi.begin();
        // Inicialização do WiFi
        const char *ssid = datas["WiFi"]["SSID"];
        const char *password = datas["WiFi"]["Password"];

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
    }

    // Inicializa protocolo GSM
    // if (data["PPP"]["Status"]){selecInter= "PPP";
    //}

    if ((datas["WiFi"]["Status"]) || (datas["PPP"]["Status"]))
    {
        initializeMQTT();
    }
}

void Interface::initializeMQTT()
{
    // Conexão com o Broker
    const char *AWS_IOT_ENDPOINT = datas["WiFi"]["AWS_IOT_ENDPOINT"];
    // Serial.println(AWS_IOT_ENDPOINT);
    const char *THINGNAME = datas["WiFi"]["THINGNAME"];
    // Serial.println(THINGNAME);

    const char AWS_IOT_SUBSCRIBE_TOPIC[] = "smartmeter/subpower";
    // Configure WiFiClientSecure to use the AWS IoT device credentials
    espClient.setCACert(AWS_CERT_CA);
    espClient.setCertificate(AWS_CERT_CRT);
    espClient.setPrivateKey(AWS_CERT_PRIVATE);

    // Connect to the MQTT broker on the AWS endpoint we defined earlier
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