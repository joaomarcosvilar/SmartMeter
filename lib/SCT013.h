#ifndef SCT013_H
#define SCT013_H
#include <Arduino.h>
#include <Adafruit_ADS1X15.h>

class SCT013
{
public:
    SCT013(Adafruit_ADS1115 *ads);
    void readIinst(int adcPin);
    void readIFreq(int adcPin);

private:
    Adafruit_ADS1115 *ads;

    // Leituras
    float I_inst, I_Offset = 0, I_coefLinear = 0;
    float start, current;
    float adcVSingleEnd, adcV_last;
    unsigned long Star = 0, Curr;
    int contt = 0, contt2 = 0;
    float freq = 0;
    bool checkVCross, lastVCross;
};


SCT013::SCT013(Adafruit_ADS1115 *ads)
{
    this->ads = ads;
}

// função alto calibração de main, min, max

void SCT013::readIinst(int adcPin)
{
    adcVSingleEnd = (ads->computeVolts(ads->readADC_SingleEnded(adcPin)));
    Serial.print(">adcVSingleEnd:");
    Serial.println((adcVSingleEnd));
}

void SCT013::readIFreq(int adcPin)
{
    // while (contt2 <= 10)
    // {
    //     adcV_last = ((ads->computeVolts(ads->readADC_SingleEnded(adcPin))) - V_sensorOffset) / V_coefLinear;
    //     Serial.print(">adcV_last:");
    //     Serial.println((adcV_last));
    //     while (contt < 2)
    //     {
    //         current = (millis());
    //         if ((current - start) >= 16)
    //         {
    //             V_inst = ((ads->computeVolts(ads->readADC_SingleEnded(adcPin))) - V_sensorOffset) / V_coefLinear;
    //             Serial.print(">V_inst:");
    //             Serial.println((V_inst));
    //             if ((V_inst > adcV_last) && (!checkVCross))
    //             {
    //                 if (contt == 0)
    //                 {
    //                     Star = micros();
    //                 }
    //                 checkVCross = true;
    //                 contt++;
    //             }
    //             else if ((V_inst < adcV_last) && (checkVCross))
    //             {
    //                 checkVCross = false;
    //             }
    //             Serial.print(">checkVCross:");
    //             Serial.println((checkVCross) * 300);
    //             start = current;
    //         }
    //     }
    //     freq += (1000000.0 / static_cast<float>(micros() - Star));
    //     Serial.println(((1000000.0 / static_cast<float>(micros() - Star))));
    //     contt = 0;
    //     contt2++;
    // }
    // Serial.print(">freq:");
    // Serial.println((freq / contt2));
    // freq = 0;
    // contt2 = 0;
    
}
#endif