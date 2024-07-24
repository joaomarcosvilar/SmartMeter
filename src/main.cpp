#include <Arduino.h>
#include <ZTMPT101b.h>
#include <SCT013.h>
#include <Adafruit_ADS1X15.h>
#include "freertos/task.h"
#include "freertos/FreeRTOS.h"
#include <RTOS.h>

Adafruit_ADS1115 ads;
ZTMPT101b sensorV;

void setup()
{
  Serial.begin(9600);
  Serial.println("Getting single-ended readings from AIN0..3");
  if (!ads.begin())
  {
    Serial.println("Failed to initialize ADS.");
    while (1)
      ;
  }
}
unsigned long start = 0, current;
void loop()
{
  current = millis();
  if ((current - start) >= 150)
  {
    sensorV.readVRMS(ads.computeVolts(ads.readADC_SingleEnded(3)));
    start = current;
  }
}
