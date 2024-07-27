#ifndef ADSREADS_H
#define ADSREADS_H

#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include <Wire.h>

class ADSreads
{
public:
    Adafruit_ADS1115 ads;
    ADSreads(int _READY_PIN);
    void begin();
    void readFreq(int channel);
    void readInst(int channel);

private:
    int READY_PIN;                           // Pino ALRT do ADS1115
    uint16_t dataRate = RATE_ADS1115_860SPS; // 860 amostras por segundo

    volatile bool newData = false;
    void IRAM_ATTR onNewDataReady(); // Função de Interrupção do pino ALRT
    static ADSreads *instance;       // Ponteiro estático para a instância

    uint16_t muxConfig;
    uint16_t translateMuxconfig(int channel);
    void setChannel(int channel);
    int lastChannel = -1; // Inicialize como um valor inválido

    float previousVoltage;
    int zeroCrossings;
    unsigned long currentTime, startTime = 0;
};

#endif

// Inicialize o ponteiro estático
ADSreads *ADSreads::instance = nullptr;

ADSreads::ADSreads(int READY_PIN)
    : READY_PIN(READY_PIN), zeroCrossings(0), startTime(0), previousVoltage(0)
{
}

void ADSreads::begin()
{
    if (!ads.begin())
    {
        Serial.println("Failed to initialize ADS.");
        while (1)
            ;
    }
    else
    {
        Serial.println("ADS initialized");
    }

    ads.setGain(GAIN_TWOTHIRDS); // 2/3x gain +/- 6.144V  1 bit = 0.1875mV
    ads.setDataRate(dataRate);

    pinMode(READY_PIN, INPUT);
    instance = this; // Atribui a instância atual ao ponteiro estático
    attachInterrupt(digitalPinToInterrupt(READY_PIN), []()
                    { if (ADSreads::instance) ADSreads::instance->onNewDataReady(); }, FALLING);

    // Começa a leitura contínua no canal inicial
    setChannel(0); // Canal inicial padrão, por exemplo, canal 0
}

void IRAM_ATTR ADSreads::onNewDataReady()
{
    if (instance)
    {
        instance->newData = true;
    }
}

uint16_t ADSreads::translateMuxconfig(int channel)
{
    switch (channel)
    {
    case 0:
        return ADS1X15_REG_CONFIG_MUX_SINGLE_0;
    case 1:
        return ADS1X15_REG_CONFIG_MUX_SINGLE_1;
    case 2:
        return ADS1X15_REG_CONFIG_MUX_SINGLE_2;
    case 3:
        return ADS1X15_REG_CONFIG_MUX_SINGLE_3;
    default:
        return ADS1X15_REG_CONFIG_MUX_SINGLE_0;
    }
}

void ADSreads::setChannel(int channel)
{
    if (channel != lastChannel)
    {
        muxConfig = translateMuxconfig(channel);
        ads.startADCReading(muxConfig, /*continuous=*/true);
        lastChannel = channel;
    }
}

void ADSreads::readFreq(int channel)
{
    setChannel(channel);

    if (!newData)
    {
        return;
    }

    float voltage = ads.computeVolts(ads.getLastConversionResults());

    if ((previousVoltage > 2.5 && voltage <= 2.5) || (previousVoltage < 2.5 && voltage >= 2.5))
    {
        zeroCrossings++;
    }

    previousVoltage = voltage;

    currentTime = millis();
    if (currentTime - startTime >= 1000)
    {
        float frequency = (zeroCrossings / 2.0) / ((currentTime - startTime) / 1000.0);
        Serial.print(">Frequency");
        Serial.print(channel);
        Serial.print(" :");
        Serial.println(frequency);

        zeroCrossings = 0;
        startTime = currentTime;
    }
}

void ADSreads::readInst(int channel)
{
    setChannel(channel);

    if (!newData)
    {
        return;
    }

    float voltage = ads.computeVolts(ads.getLastConversionResults());
    Serial.print(">Vinst:");
    Serial.println(voltage);
    newData = false;
}
