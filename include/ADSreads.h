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
    float readRMS(int channel);
    void calibration(int channel);

private:
    int READY_PIN;                           // Pino ALRT do ADS1115
    uint16_t dataRate = RATE_ADS1115_860SPS; // 860 amostras por segundo
    int samples = 860;

    volatile bool newData = false;
    void IRAM_ATTR onNewDataReady(); // Função de Interrupção do pino ALRT
    static ADSreads *instance;       // Ponteiro estático para a instância
    uint8_t adress;

    uint16_t muxConfig;
    uint16_t translateMuxconfig(int channel);
    void setChannel(int channel);
    int lastChannel = -1; // Inicialize como um valor inválido

    float previousVoltage;
    int zeroCrossings, contt = 0;
    unsigned long currentTime, startTime = 0;

    float voltage, frequency, samplesRMS[860]; // Modificar o samplesRMS para config do ADS, 860 <- RATE_ADS1115_860SPS
    int index = 0;
    bool calibrate[4] = {false, false, false, false};
    float offSet[4], coefLinear[4], voltMax = 0.0, voltMin = 5.0, sumRMS = 0, RMS;
};

#endif

ADSreads *ADSreads::instance = nullptr;

ADSreads::ADSreads(int READY_PIN, uint8_t _adress)
    : READY_PIN(READY_PIN), zeroCrossings(0), startTime(0), previousVoltage(0), adress(_adress)
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
        Serial.print("ADS initialized: ");
        Serial.println(adress);
    }

    ads.setGain(GAIN_TWOTHIRDS); // 2/3x gain +/- 6.144V  1 bit = 0.1875mV
    ads.setDataRate(dataRate);

    pinMode(READY_PIN, INPUT_PULLUP);
    instance = this; // Atribui a instância atual ao ponteiro estático
    attachInterrupt(digitalPinToInterrupt(READY_PIN), []()
                    { ADSreads::instance->onNewDataReady(); }, FALLING);

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

float ADSreads::readFreq(int channel)
{
    // setChannel(channel);
    // if (!calibrate[channel])
    // {
    //     calibration(channel);
    // }
    zeroCrossings = 0;
    index = 0;
    unsigned long startMillis = millis();

    while (index < (samples + 1))
    {
        voltage = readInst(channel);
        // if ((previousVoltage > offSet[channel] && voltage <= offSet[channel]) || (previousVoltage < offSet[channel] && voltage >= offSet[channel]))
        if ((previousVoltage > 2.5 && voltage <= 2.5) || (previousVoltage < 2.5 && voltage >= 2.5))
        {
            zeroCrossings++;
        }
        previousVoltage = voltage;
        index++;
    }

    frequency = (zeroCrossings / 2.0) / ((millis() - startMillis) / 1000.0); // fazer tratamento para quando for desligado
    Serial.print(">Frequency");
    Serial.print(channel);
    Serial.print(" :");
    Serial.println(frequency);
    return frequency;

    // condição de leitura diferente de 60Hz
}

float ADSreads::readInst(int channel)
{
    setChannel(channel);
    while (1)
    {
        Serial.print("Aqui");
        if (newData)
        {
            break;
        }
    }
    // voltage = ads.computeVolts(ads.getLastConversionResults());
    // Serial.print(">Vinst:");
    // Serial.println(voltage);
    newData = false;
    return ads.computeVolts(ads.getLastConversionResults());
}

// Aquisição de várias amostras do sinal, cálculo do quadrado de cada amostra,
// média desses valores quadrados e, finalmente, extração da raiz quadrada da média.
float ADSreads::readRMS(int channel)
{
    index = 0;
    sumRMS = 0;
    // if (!calibrate[channel])
    // {
    //     calibration(channel);
    // }
    while (index < (samples + 1))
    {
        voltage = readInst(channel);
        sumRMS += ((voltage - offSet[channel]) * coefLinear[channel]) * ((voltage - offSet[channel]) * coefLinear[channel]);
        index++;
    }

    RMS = sqrt(sumRMS / samples);
    // Serial.print(">RMS:");
    // Serial.println(RMS, 6);
    return RMS;
}

void ADSreads::calibration(int channel)
{
    voltMin = 5.0;
    voltMax = 0.0;
    contt = 0;
    Serial.println("Calibration ");
    while (contt < samples)
    {
        readInst(channel);
        if (voltage < voltMin)
        {
            voltMin = voltage;
        }
        if (voltage > voltMax)
        {
            voltMax = voltage;
        }
        contt++;
    }
    Serial.println(voltMax, 6);
    Serial.println(voltMin, 6);
    offSet[channel] = (voltMax + voltMin) / 2.0;
    Serial.println(offSet[channel], 6);
    // identificador de sensor acoplado

    if (channel == 3)
    {
        coefLinear[3] = 253.751; // Ajusta coefLinear para o canal 3
    }
    else if (channel == 2)
    {
        coefLinear[2] = 30.769; // Ajusta coefLinear para o canal 2
    }
    calibrate[channel] = true;
}
