#ifndef ZTMPT101B_H
#define ZTMPT101B_H
#include <Arduino.h>

class ZTMPT101b
{
public:
    float readVRMS(int16_t adcVSingleEnd);

private:
    float voltageSampleRead = 0;  /* to read the value of a sample*/
    float voltageLastSample = 0;  /* to count time for each sample. Technically 1 milli second 1 sample is taken */
    float voltageSampleSum = 0;   /* accumulation of sample readings */
    float voltageSampleCount = 0; /* to count number of sample. */
    float voltageMean;            /* to calculate the average value from all samples*/
    float RMSVoltageMean;         /* square roof of voltageMean*/
    float voltageOffset1 = 0.00;     // to Offset deviation and accuracy. Offset any fake current when no current operates.
                                  // Offset will automatically callibrate when SELECT Button on the LCD Display Shield is pressed.
                                  // If you do not have LCD Display Shield, look into serial monitor to add or minus the value manually and key in here.
    float voltageOffset2 = 0;     // to offset value due to calculation error from squared and square root.
    float calbrationFactor = 1.5;
};

#endif