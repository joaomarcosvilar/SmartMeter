#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>

#include "../include/ADSreads.h"
#include "../include/LoRaMEshLib.h"

#define vREADY_PIN 23
ADSreads SensorV(vREADY_PIN, 0x48);

#define iREADY_PIN 19
ADSreads SensorI(iREADY_PIN, 0x4B);

void IRAM_ATTR onVReady()
{
  SensorV.onNewDataReady();
}

void IRAM_ATTR onIReady()
{
  SensorI.onNewDataReady();
}

LoRaEnd lora(17, 16);

DynamicJsonDocument data(253);
String str;

void setup()
{
  Serial.begin(115200);

  SensorV.begin();
  SensorI.begin();
  attachInterrupt(digitalPinToInterrupt(vREADY_PIN), onVReady, FALLING);
  attachInterrupt(digitalPinToInterrupt(iREADY_PIN), onIReady, FALLING);

  delay(1000);
  lora.begin(9600);
}

int contt = 0;
unsigned long currentTime, startTime = 0;
void loop()
{
  // Serial.print(">Vinst:");
  // Serial.println(SensorI.readInst(0));
  currentTime = millis();
  if ((currentTime - startTime) >= 5000)
  {
    float leitura = SensorV.readInst(0);
    char leituraString[8];
    dtostrf(leitura,4,2,leituraString);
    data["V"] = leituraString;
    data["I"] = SensorI.readInst(3);
    data["F"] = SensorV.readFreq(0);

    serializeJson(data, str);

    if (lora.idRead() == 1)
    {
      lora.sendMaster(str);
    }
    startTime = currentTime;
  }
}
