#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>

#include "../include/ADSreads.h"
#include "../include/LoRaMEshLib.h"

#define READY_PIN 23
ADSreads Sensor(READY_PIN);

LoRaEnd lora(17, 16);

StaticJsonDocument<253> data;
char str[253];

void setup()
{
  Serial.begin(115200);
  Sensor.begin();
  lora.begin(9600);
}

unsigned long currentTime, startTime = 0;
void loop()
{
  currentTime = millis();
  if ((currentTime - startTime) >= 5000)
  {
    data["V"] = Sensor.readRMS(3);
    data["I"] = Sensor.readRMS(2);
    data["F"] = Sensor.readFreq(3);

    serializeJson(data,str);

    lora.sendMaster(str);
    startTime = currentTime;
  }
}
