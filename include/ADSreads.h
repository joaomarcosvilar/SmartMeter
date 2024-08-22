#ifndef ADSREADS_H
#define ADSREADS_H

#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include <Wire.h>

class ADSreads
{
public:
    Adafruit_ADS1115 ads;
    ADSreads(int _READY_PIN, uint8_t _adress);
    void begin();
    float readFreq(int channel);
    float readInst(int channel);
    float readRMS(int channel, float a, float b);
    void IRAM_ATTR onNewDataReady(); // Função de Interrupção do pino ALRT
    float readADC(int channel);
    float readRealIns(int channel, float a, float b);

private:
    int READY_PIN;                           // Pino ALRT do ADS1115
    uint16_t dataRate = RATE_ADS1115_860SPS; // 860 amostras por segundo
    int samples = 860;

    volatile bool newData = false;

    static ADSreads *instance; // Ponteiro estático para a instância
    uint8_t adress;

    uint16_t muxConfig;
    uint16_t translateMuxconfig(int channel);
    void setChannel(int channel);
    int lastChannel = -1;

    bool avaliable[4] = {true, true, true, true};
    float toleranceVoltage = 0.1; // Teste para identificar se o sensor está conectado e funcionando corretamente
};

#endif

ADSreads *ADSreads::instance = nullptr;

ADSreads::ADSreads(int READY_PIN, uint8_t _adress)
    : READY_PIN(READY_PIN), adress(_adress)
{
}

void ADSreads::begin()
{
    if (!ads.begin(adress))
    {
        Serial.print("Failed to initialize ADS:");
        Serial.println(adress);
        while (1)
            ;
    }
    else
    {
        // Serial.print("ADS initialized: ");
        // Serial.println(adress);
    }

    // ads.setGain(GAIN_TWOTHIRDS); // 2/3x gain +/- 6.144V  1 bit = 0.1875mV   <- fazendo ocorrer um overshoot na leitura
    ads.setDataRate(dataRate);

    pinMode(READY_PIN, INPUT_PULLUP);
}

void IRAM_ATTR ADSreads::onNewDataReady()
{
    newData = true;
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

float ADSreads::readFreq(int channel)
{
    int zeroCrossings = 0;
    int index = 0;
    float voltage = 0, previousVoltage = 0;
    if (avaliable[channel])
    {
        setChannel(channel);
        while (index < (samples + 1))
        {
            voltage += readInst(channel);
            index++;
        }
        float mean = voltage / samples;

        index = 0;
        unsigned long startMillis = millis();
        while (index < (samples + 1))
        {
            voltage = readInst(channel);
            if ((previousVoltage > mean && voltage <= mean) || (previousVoltage < mean && voltage >= mean))
            {
                zeroCrossings++;
            }
            previousVoltage = voltage;
            index++;
        }

        float frequency = (zeroCrossings / 2.0) / ((millis() - startMillis) / 1000.0);
        return frequency;
    }
    else
    {
        Serial.println("Sensor não reconhecido!RMS");
        return 0;
    }
}

float ADSreads::readInst(int channel)
{
    float voltMax = 0.0, voltMin = 5.0;
    setChannel(channel);
    unsigned long startTime = millis();

    while (millis() - startTime < 500)
    {
        if (newData)
        {
            newData = false;
            float voltage = ads.computeVolts(ads.getLastConversionResults());
            // if (voltage < voltMin)
            // {
            //     voltMin = voltage;
            // }
            // if (voltage > voltMax)
            // {
            //     voltMax = voltage;
            // }
            // if ((voltMax - voltMin) < toleranceVoltage)
            // {
            //     avaliable[channel] = false;
            //     Serial.println("Tolerancia não alcançada.");
            //     return 0;
            // }
            // Serial.println(voltage);
            return voltage;
        }
    }
    Serial.println("ADC error in readInst");
    return 0;
    // }
    // else
    // {
    //     Serial.println("Sensor não reconhecido!INST");
    //     return 0;
    // }
}

float ADSreads::readRMS(int channel, float a, float b)
{
    // if (avaliable[channel])
    // {

    // TODO: verificar de outra forma refinada
    if (a == 1 && b == 0)
    { // Canal não calibrado
        return 0;
    }

    float sum = 0;
    for (int i = 0; i < samples; i++)
    {
        float instValue = readInst(channel);
        // Aplicar coeficientes de calibração
        sum += instValue * instValue;
    }
    float trueRMS = sqrt(sum / samples);
    trueRMS = trueRMS*a + b;
    return trueRMS;

    // while (index < (samples + 1))
    // {
    //     float voltage = readInst(channel);
    //     // Serial.println(String(a * voltage + b));
    //     sumRMS += voltage * voltage;
    //     index++;
    // }
    // float RMS = sumRMS / index;
    // RMS = sqrt(RMS);
    // RMS = a * RMS + b;

    // return RMS;
    // }
    // else
    // {
    //     Serial.println("Sensor não reconhecido!");
    //     return 0;
    // }
}

float ADSreads::readADC(int channel)
{
    if (avaliable[channel])
    {
        setChannel(channel);
        unsigned long startTime = millis();

        while (millis() - startTime < 500)
        {
            if (newData)
            {
                newData = false;
                return (ads.getLastConversionResults());
            }
        }
        Serial.println("ADC error in readADC");
        return 0;
    }
    else
    {
        Serial.println("Sensor não reconhecido!");
        return 0;
    }
}

float ADSreads::readRealIns(int channel, float a, float b)
{
    float voltage = readInst(channel);
    return (a * voltage + b);
}