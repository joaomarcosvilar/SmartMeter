#ifndef SCT013_H
#define SCT013_H
#include <Arduino.h>
class SCT013
{
public:
    float readI(int16_t adcSingleEnd);

private:
    float mVperAmpValue = 16.5;    // If using ACS712 current module : for 5A module key in 185, for 20A module key in 100, for 30A module key in 66
                                    // If using "Hall-Effect" Current Transformer, key in value using this formula: mVperAmp = maximum voltage range (in milli volt) / current rating of CT
                                    /* For example, a 20A Hall-Effect Current Transformer rated at 20A, 2.5V +/- 0.625V, mVperAmp will be 625 mV / 20A = 31.25mV/A */
    float currentSampleRead = 0;    /* to read the value of a sample*/
    float currentLastSample = 0;    /* to count time for each sample. Technically 1 milli second 1 sample is taken */
    float currentSampleSum = 0;     /* accumulation of sample readings */
    float currentSampleCount = 0;   /* to count number of sample. */
    float currentMean;              /* to calculate the average value from all samples*/
    float RMSCurrentMean = 0;       /* square roof of currentMean*/
    float FinalRMSCurrent;          /* the final RMS current reading*/

    /*2.1 Offset AC Current */
    float calibrationFactor = 1;
    float currentOffset1 = 0; // to Offset deviation and accuracy. Offset any fake current when no current operates.
                              // Offset will automatically callibrate when SELECT Button on the LCD Display Shield is pressed.
                              // If you do not have LCD Display Shield, look into serial monitor to add or minus the value manually and key in here.
                              // 26 means add 26 to all analog value measured
    float currentOffset2 = 0; // to offset value due to calculation error from squared and square root.
};

#endif