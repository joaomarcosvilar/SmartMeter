#ifndef ZTMPT101B_H
#define ZTMPT101B_H
#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include "freertos/task.h"
#include "freertos/FreeRTOS.h"

class ZTMPT101b
{
public:
    ZTMPT101b(Adafruit_ADS1115 *ads, float Vrms, int pinALRT);
    void begin();
    void startContinuousReading(int channelADC);

    void readVinst();
    void readFreq(int channelADC);

private:
    // Setup
    Adafruit_ADS1115 *ads;
    int channelADC;
    int pinALRT;

    // Timers
    unsigned long Star = 0, Curr, start = 0, current;

    // Frequencia
    int contt = 0, contt2 = 0;
    float freq = 0;
    bool checkVCross, lastVCross;

    // Calibração e Leitura de Tensão
    void calibrateSensor(int channelADC, float Vrms);
    int numCalibrationSamples = 10;
    float minVoltage = 5.0, maxVoltage = 0.0, V_sensorOffset = 2.5, V_coefLinear = 0.0026937;
    float V_inst;
    float adcVSingleEnd, adcV_last;

    // Leitura continua ADS
    static void IRAM_ATTR newDataReadyISR();
    volatile bool newData;
    int16_t lastResult;
    void update();
};

ZTMPT101b *instance = nullptr;

ZTMPT101b::ZTMPT101b(Adafruit_ADS1115 *ads, float Vrms, int _pinALRT)
    : ads(ads), V_inst(0), newData(false), lastResult(0)
{
    instance = this;
    int channelADC = 3;
    calibrateSensor(channelADC, Vrms);
    pinALRT = _pinALRT;
}

void ZTMPT101b::begin()
{
    if (!ads->begin())
    {
        Serial.println("Failed to initialize ADS.");
        while (1)
            ;
    }

    // Configura o pino de interrupção
    pinMode(pinALRT, INPUT);
    attachInterrupt(digitalPinToInterrupt(pinALRT), newDataReadyISR, FALLING);
}

void ZTMPT101b::startContinuousReading(int channelADC)
{
    this->channelADC = channelADC;
    ads->startADCReading(ADS1X15_REG_CONFIG_MUX_SINGLE_0 + channelADC, true);
}

void ZTMPT101b::update()
{
    if (newData)
    {
        lastResult = ads->getLastConversionResults();
        newData = false;
    }
}

void IRAM_ATTR ZTMPT101b::newDataReadyISR()
{
    instance->newData = true;
}

void ZTMPT101b::readVinst()
{
    update();
    adcVSingleEnd = ads->computeVolts(lastResult);
    V_inst = (adcVSingleEnd - V_sensorOffset) / V_coefLinear;
    Serial.print(">V_inst:");
    Serial.println(V_inst);
}

void ZTMPT101b::calibrateSensor(int channelADC, float Vrms)
{
    for (int i = 0; i < numCalibrationSamples; i++)
    {
        update();
        adcVSingleEnd = (ads->computeVolts(ads->readADC_SingleEnded(channelADC)));

        if (adcVSingleEnd < minVoltage)
        {
            minVoltage = adcVSingleEnd;
        }
        if (adcVSingleEnd > maxVoltage)
        {
            maxVoltage = adcVSingleEnd;
        }
        delay(1);
    }

    V_sensorOffset = (minVoltage + maxVoltage) / 2.0;
    V_coefLinear = (maxVoltage - V_sensorOffset) / (Vrms * sqrt(2));
}


void ZTMPT101b::readFreq(int channelADC)
{
    while (contt2 <= 10)
    {
        update();
        adcV_last = ((ads->computeVolts(ads->readADC_SingleEnded(channelADC))) - V_sensorOffset) / V_coefLinear;
        Serial.print(">adcV_last:");
        Serial.println((adcV_last));
        while (contt < 2)
        {
            current = (millis());
            if ((current - start) >= 16)
            {
                V_inst = ((ads->computeVolts(ads->readADC_SingleEnded(channelADC))) - V_sensorOffset) / V_coefLinear;
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
