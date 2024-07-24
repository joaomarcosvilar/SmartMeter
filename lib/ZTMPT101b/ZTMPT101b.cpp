#include "ZTMPT101b.h"
#include <Arduino.h>
#include "Emonlib.h"

EnergyMonitor V;

float ZTMPT101b::readVRMS(int16_t adcVSingleEnd)
{

    {//Tentativas anteriores
    // if (millis() >= voltageLastSample + 500) /* every 1 milli second taking 1 reading */
    // {
    //     voltageSampleRead = 2 * (adcSingleEnd - 512) + voltageOffset1; /* read the sample value */
    //     voltageSampleSum = voltageSampleSum + sq(voltageSampleRead);   /* accumulate value with older sample readings*/
    //     voltageSampleCount = voltageSampleCount + 1;                   /* to move on to the next following count */
    //     voltageLastSample = millis();                                  /* to reset the time again so that next cycle can start again*/
    // }

    // if (voltageSampleCount == 10) /* after 1000 count or 1000 milli seconds (1 second), do the calculation and display value*/
    // {
    //     voltageMean = voltageSampleSum / voltageSampleCount;                      /* calculate average value of all sample readings taken*/
    //     RMSVoltageMean = (sqrt(voltageMean)) * calbrationFactor + voltageOffset2; /* square root of the average value*/
    //     Serial.print(RMSVoltageMean, 6);
    //     Serial.println(" V   ");
    //     voltageSampleSum = 0;   /* to reset accumulate sample values for the next cycle */
    //     voltageSampleCount = 0; /* to reset number of sample for the next cycle */
    //     return RMSVoltageMean;
    // }
    // voltageSampleRead = (311.13 * (adcVSingleEnd - 2.53)) / 3.374541;
    // voltageSampleSum += voltageSampleRead;
    // voltageSampleCount += 1;
    // Serial.print("V_inst = ");
    // Serial.println(voltageSampleRead, 6);
    // if (voltageSampleCount >= 10)
    // {
    //     voltageMean = voltageSampleSum / voltageSampleCount;
    //     RMSVoltageMean = (voltageMean * (3.14159 / 2.0)) / sqrt(2); // (Vpico = π/2 * Vmedia)/raiz(2)
    //     Serial.print("V_mean = ");
    //     Serial.println(voltageMean, 6);
    //     Serial.print("V_rms = ");
    //     Serial.println(RMSVoltageMean, 6);
    //     voltageSampleCount = 0;
    //     voltageSampleSum = 0;
    // }
    // return 0;
    }

    V.voltage(adcVSingleEnd, 870.0, 0);
    V.calcVI(20, 2000);                      // Calculate V and I, number of crossings, timeout
    double Vrms = V.Vrms;                    // Extract Vrms
    Serial.print("Tensão RMS: ");
    Serial.print(Vrms, 4); // Print Vrms with 4 decimal places
    Serial.println(" V");

    return 0;
}