#include <Arduino.h>
#include <Adafruit_ADS1X15.h>

#include "../include/ADSreads.h"

#define READY_PIN 23
ADSreads Sensor(READY_PIN);

void setup()
{
  Serial.begin(115200);
  Sensor.begin();
}

unsigned long currentTime, startTime = 0;
void loop()
{
  currentTime = millis();
  if ((currentTime - startTime) >= 1000)
  {
    Sensor.readRMS(2);
    while(1);
    startTime = currentTime;
  }
}
