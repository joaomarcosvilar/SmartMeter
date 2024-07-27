#include <Arduino.h>
#include <Adafruit_ADS1X15.h>

#include "../include/ADSreads.h"

#define READY_PIN 23
ADSreads Sensor(READY_PIN);

void setup()
{
  Serial.begin(115200);

}


void loop()
{
  Sensor.readFreq(3);
}
