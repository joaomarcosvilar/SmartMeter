#include "../include/SCT013.h"
#include <Arduino.h>
float SCT013::readI(int16_t adcPIN){
    while(true){
     if(millis() >= currentLastSample + 1)                                                     /* every 1 milli second taking 1 reading */
          {
            currentSampleRead = analogRead(CurrentAnalogInputPin)-512 + currentOffset1;           /* read the sample value */
            currentSampleSum = currentSampleSum + sq(currentSampleRead) ;                         /* accumulate value with older sample readings*/
            currentSampleCount = currentSampleCount + 1;                                          /* to move on to the next following count */
            currentLastSample = millis();                                                         /* to reset the time again so that next cycle can start again*/ 
          }
        
        if(currentSampleCount == 1000)                                                            /* after 1000 count or 1000 milli seconds (1 second), do the calculation and display value*/
          {
            currentMean = currentSampleSum/currentSampleCount;                                    /* calculate average value of all sample readings taken*/
            RMSCurrentMean = (sqrt(currentMean))*calibrationFactor + currentOffset2 ;                                   /* square root of the average value*/
            FinalRMSCurrent = (((RMSCurrentMean /1024) *5000) /mVperAmpValue);                    /* calculate the final RMS current*/
            Serial.print(FinalRMSCurrent,decimalPrecision*2);
            Serial.print(" A   ");
            currentSampleSum =0;                                                                  /* to reset accumulate sample values for the next cycle */
            currentSampleCount=0;                                                                 /* to reset number of sample for the next cycle */
            return FinalRMSCurrent;
          }
    }
}