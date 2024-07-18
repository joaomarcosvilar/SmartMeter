// AC Voltage Sensor without LCD By Solarduino (Revision 2)

// Note Summary
// Note :  Safety is very important when dealing with electricity. We take no responsibilities while you do it at your own risk.
// Note :  This AC Votlage Sensor Code is for Single Phase AC Voltage transformer ZMPT101B module use.
// Note :  The value shown in Serial Monitor is refreshed every second and is the average value of 4000 sample readings.
// Note :  The voltage measured is the Root Mean Square (RMS) value.
// Note :  The analog value per sample is squared and accumulated for every 4000 samples before being averaged. The averaged value is then getting square-rooted.
// Note :  The auto calibration (voltageOffset1) is using averaged analogRead value of 4000 samples.
// Note :  The auto calibration (currentOffset2) is using calculated RMS current value including currentOffset1 value for calibration.
// Note :  The unit provides reasonable accuracy and may not be comparable with other expensive branded and commercial product.
// Note :  All credit shall be given to Solarduino.
//https://solarduino.com/how-to-measure-ac-voltage-with-arduino/
//https://www.fernandok.com/2022/04/medidor-de-tensao-e-corrente-ac-com.html
/*/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/ ////////////*/

#include <Arduino.h>

/* 0- General */

int decimalPrecision = 2; // decimal places for all values shown in LED Display & Serial Monitor

/* 1- AC Voltage Measurement */

int VoltageAnalogInputPin = 13; // Which pin to measure voltage Value (Pin A0 is reserved for button function)
float voltageSampleRead = 0;    /* to read the value of a sample in analog including voltageOffset1 */
float voltageLastSample = 0;    /* to count time for each sample. Technically 1 milli second 1 sample is taken */
float voltageSampleSum = 0;     /* accumulation of sample readings */
float voltageSampleCount = 0;   /* to count number of sample. */
float voltageMean;              /* to calculate the average value from all samples, in analog values*/
float RMSVoltageMean;           /* square roof of voltageMean without offset value, in analog value*/
float adjustRMSVoltageMean;
float FinalRMSVoltage; /* final voltage value with offset value*/

/* 1.1- AC Voltage Offset */

float voltageOffset1 = -1070.00; // to Offset deviation and accuracy. Offset any fake current when no current operates.
                                 // Offset will automatically callibrate when SELECT Button on the LCD Display Shield is pressed.
                                 // If you do not have LCD Display Shield, look into serial monitor to add or minus the value manually and key in here.
                                 // 26 means add 26 to all analog value measured.
float voltageOffset2 = 0.00;     // too offset value due to calculation error from squared and square root
float calbrationFactor = 1.2;

void readRMS()

{

    /* 1- AC Voltage Measurement */
    if (micros() >= voltageLastSample + 1000) /* every 0.2 milli second taking 1 reading */
    {
        voltageSampleRead = (analogRead(VoltageAnalogInputPin) - 512) + voltageOffset1; /* read the sample value including offset value*/
        voltageSampleSum = voltageSampleSum + sq(voltageSampleRead);                    /* accumulate total analog values for each sample readings*/

        voltageSampleCount = voltageSampleCount + 1; /* to move on to the next following count */
        voltageLastSample = micros();                /* to reset the time again so that next cycle can start again*/
    }

    if (voltageSampleCount == 1000) /* after 4000 count or 800 milli seconds (0.8 second), do the calculation and display value*/
    {
        voltageMean = voltageSampleSum / voltageSampleCount;     /* calculate average value of all sample readings taken*/
        RMSVoltageMean = (sqrt(voltageMean)) * calbrationFactor; // The value X 1.5 means the ratio towards the module amplification.
        adjustRMSVoltageMean = RMSVoltageMean + voltageOffset2;  /* square root of the average value including offset value */
        // Serial.println("adjustRMS: " + String(adjustRMSVoltageMean));                                                                                                                                                       /* square root of the average value*/
        FinalRMSVoltage = RMSVoltageMean + voltageOffset2; /* this is the final RMS voltage*/
        if (FinalRMSVoltage <= 3)                          /* to eliminate any possible ghost value*/
        {
            FinalRMSVoltage = 0;
        }
        Serial.print(" The Voltage RMS value is: ");
        Serial.print(FinalRMSVoltage, decimalPrecision);
        Serial.println(" V ");
        voltageSampleSum = 0;   /* to reset accumulate sample values for the next cycle */
        voltageSampleCount = 0; /* to reset number of sample for the next cycle */
    }
}