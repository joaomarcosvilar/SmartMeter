#ifndef ZTMPT101B_H
#define ZTMPT101B_H
#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
class ZTMPT101b
{
public:
    ZTMPT101b(Adafruit_ADS1115 *ads);
    void readVinst(int adcPin);
    void readFreq(int adcPin);

private:
    // Setup
    Adafruit_ADS1115 *ads;

    // Leitura
    float V_inst;
    float V_sensorOffset = 2.5;     // Tensão lida do sensor quando não conectado na energia
    float V_coefLinear = 0.0026937; // Coeficiente linear proporcional
    

    // Freq
    float adcVSingleEnd, adcV_last;
    unsigned long Star = 0, Curr;
    float start, current;
    int contt = 0, contt2 = 0;
    float freq = 0;
    bool checkVCross, lastVCross;
};

ZTMPT101b::ZTMPT101b(Adafruit_ADS1115 *ads)
{
    this->ads = ads;
}

// função alto calibração de main, min, max

void ZTMPT101b::readVinst(int adcPin)
{
    adcVSingleEnd = (ads->computeVolts(ads->readADC_SingleEnded(adcPin)));
    V_inst = (adcVSingleEnd - V_sensorOffset) / V_coefLinear;
    Serial.print(">V_inst:");
    Serial.println((V_inst));
}

void ZTMPT101b::readFreq(int adcPin)
{
    while (contt2 <= 10)
    {
        adcV_last = ((ads->computeVolts(ads->readADC_SingleEnded(adcPin))) - V_sensorOffset) / V_coefLinear;
        Serial.print(">adcV_last:");
        Serial.println((adcV_last));
        while (contt < 2)
        {
            current = (millis());
            if ((current - start) >= 16)
            {
                V_inst = ((ads->computeVolts(ads->readADC_SingleEnded(adcPin))) - V_sensorOffset) / V_coefLinear;
                Serial.print(">V_inst:");
                Serial.println((V_inst));
                if ((V_inst > adcV_last) && (!checkVCross))
                {
                    if (contt == 0)
                    {
                        Star = micros();
                    }
                    checkVCross = true;
                    contt++;
                }
                else if ((V_inst < adcV_last) && (checkVCross))
                {
                    checkVCross = false;
                }
                Serial.print(">checkVCross:");
                Serial.println((checkVCross) * 300);
                start = current;
            }
        }
        freq += (1000000.0 / static_cast<float>(micros() - Star));
        Serial.println(((1000000.0 / static_cast<float>(micros() - Star))));
        contt = 0;
        contt2++;
    }
    Serial.print(">freq:");
    Serial.println((freq / contt2));
    freq = 0;
    contt2 = 0;
    
}

#endif
