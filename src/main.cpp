#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include <LoRaMESH.h>

#include "../lib/ZTMPT101b.h"

#include "freertos/task.h"
#include "freertos/FreeRTOS.h"

#define pinALRT 23

Adafruit_ADS1115 ads;
ZTMPT101b sensorV(&ads, 220, pinALRT);

void setup()
{
  Serial.begin(115200);
  sensorV.begin();
  sensorV.startContinuousReading(0);
}

unsigned long start = 0, current;

void loop()
{
  // current = static_cast<float>(millis());
  // if ((current - start) >= (1 / 60))
  // {
  //   // ads.computeVolts(ads.readADC_SingleEnded(3))
  //   // sensorV.readVinst(3);
  //   sensorV.readFreq(3);
  //   // Serial.print(">adcVSingleEnd:");
  //   // Serial.println(ads.computeVolts(ads.readADC_SingleEnded(3)));
  //   start = current;
  // }
}
