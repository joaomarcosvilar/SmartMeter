#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include <LoRaMESH.h>

#include "../lib/ZTMPT101b.h"
#include "../lib/SCT013.h"

#include "freertos/task.h"
#include "freertos/FreeRTOS.h"

Adafruit_ADS1115 ads;
ZTMPT101b sensorV(&ads);
SCT013 sensorI(&ads);

void setup()
{
  Serial.begin(115200);

  if (!ads.begin())
  {
    Serial.println("Failed to initialize ADS.");
    while (1)
      ;
  }
  else
  {
    Serial.println("ADS inicializado...");
  }
}

unsigned long start = 0, current;

void loop()
{
  current = micros();
  if ((current - start) >= 16)
  {
    // ads.computeVolts(ads.readADC_SingleEnded(3))
    sensorV.readVinst(3);
    //sensorV.readFreq(3);
    //sensorI.readIinst(2);
    // Serial.print(">adcVSingleEnd:");
    // Serial.println(ads.computeVolts(ads.readADC_SingleEnded(3)));
    start = current;
  }
}
