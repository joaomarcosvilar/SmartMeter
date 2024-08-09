#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>

#include "../include/ADSreads.h"
#include "../include/LoRaMEshLib.h"

#define vREADY_PIN 18
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
  Serial.print("Ready?");
  while (Serial.available() == 0)
  {
  }
  // as interrupções deve ocorrer antes para ocorrer a calibração, utilizando os dados da leitura instantanea
  attachInterrupt(digitalPinToInterrupt(vREADY_PIN), onVReady, FALLING);
  attachInterrupt(digitalPinToInterrupt(iREADY_PIN), onIReady, FALLING);

  SensorV.begin();
  SensorI.begin();

  Wire.setBufferSize(256);
  delay(2000);
  // lora.begin(9600);

  // SensorV.calibration(0);
  SensorV.calibration(0);
}
int contt = 0;
struct coleta
{
  unsigned long timer;
  float vinst;
};
coleta leitura[2000];
float vinst2;
unsigned long currentTime, startTime = 0;
void loop()
{
  // Serial.print(">Vinst:");
  // Serial.println(SensorI.readInst(0));
  // currentTime = millis();
  // if ((currentTime - startTime) >= 5000)
  // {
  //   float leitura = SensorV.readInst(0);
  //   char leituraString[8];
  //   dtostrf(leitura,4,2,leituraString);
  //   data["V"] = leituraString;
  //   data["I"] = SensorI.readInst(3);
  //   data["F"] = SensorV.readFreq(0);

  //   serializeJson(data, str);

  //   if (lora.idRead() == 1)
  //   {
  //     lora.sendMaster(str);
  //   }
  //   startTime = currentTime;
  // }

  // while (contt < 2001)
  // {
  //   leitura[contt].timer = millis();
  //   leitura[contt].vinst = SensorI.readInst(0);
  //   contt++;
  // }
  // contt = 0;
  // while (contt < 2001)
  // {
  //   if ((millis() - startTime) > 10)
  //   {
  //     Serial.print(leitura[contt].timer);
  //     Serial.print("|");
  //     Serial.print(leitura[contt].vinst, 6);
  //     Serial.print(",");
  //     contt++;
  //   }
  // }
  // while (1)
  //   ;

  currentTime = millis();
  if ((currentTime - startTime) >= 500)
  {
    Serial.print(SensorV.readInst(0), 6);
    Serial.print(" : ");
    Serial.println(SensorV.readRMS(0),6);
    startTime = currentTime;
  }
}