#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>

#include "../include/ADSreads.h"
#include "../include/LoRaMEshLib.h"

#define vREADY_PIN 23
ADSreads SensorV(vREADY_PIN, 0x48);

#define iREADY_PIN 19
ADSreads SensorI(iREADY_PIN, 0x4A);

// LoRaEnd lora(16,17);

DynamicJsonDocument data(253);
String str;

// void IRAM_ATTR ADSreads::onNewDataReady()
// {
//     if (instance)
//     {
//         instance->newData = true;
//     }
// } 
// implementar update para receber a newdata


void setup()
{
  Serial.begin(115200);
  SensorV.begin();
  SensorI.begin();
  delay(1000);
  // lora.begin(9600);
  // attachInterrupt(digitalPinToInterrupt(READY_PIN), []()
  //                   { if (ADSreads::instance) ADSreads::instance->onNewDataReady(); }, FALLING); trabalhar com os dois pinos
}

unsigned long currentTime, startTime = 0;
void loop()
{
  currentTime = millis();
 
  if ((currentTime - startTime) >= 500)
  {
    // data["V"] = SensorV.readInst(0);
    // data["I"] = SensorV.readInst(3);
    // data["F"] = SensorV.readFreq(0);

    // serializeJson(data,str);
    Serial.println(SensorV.readInst(0));
    Serial.println(SensorI.readInst(0));
    // lora.sendMaster(str);
    startTime = currentTime;
  }
}
