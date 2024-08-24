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
    void begin(int gain);
    float readFreq(int channel);
    float readInst(int channel);
    float readRMS(int channel, float coefficients[]);
    void IRAM_ATTR onNewDataReady(); // Função de Interrupção do pino ALRT
    float readADC(int channel);
    // float readRealIns(int channel, float a, float b);

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

void ADSreads::begin(int gain)
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

    if(gain == 1){
        ads.setGain(GAIN_TWOTHIRDS); //DEFAULt 2/3x gain +/- 6.144V  1 bit = 0.1875mV
    }
    if(gain == 2){
        ads.setGain(GAIN_TWO); // 16x gain  +/- 0.256V  1 bit = 0.125mV  0.0078125mV
    }
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
            return voltage;
        }
    }
    return 0;
}

float ADSreads::readRMS(int channel, float coefficients[])
{
    int DEGREE = 3;

    // Verifica se todos os coeficientes estão no valor padrão 0
    // int index = 0;
    // for(int i = 0; i< DEGREE ;i++){
    //     if(coefficients[i]==0) index++;
    // }
    // if(index == 4) return 0;


    float sum = 0;
    for (int i = 0; i < samples; i++)
    {
        float instValue = readInst(channel);
        sum += instValue * instValue;
    }
    float sensorRMS = sqrt(sum / samples);
    // Serial.println(sensorRMS,6);
    // Serial.print(coefficients[0]);Serial.print(";");Serial.print(coefficients[1]);Serial.print(";");Serial.println(coefficients[2]);
    // Usando polinomio de grau 3
    float trueRMS = coefficients[2] 
                 + coefficients[1] * sensorRMS 
                 + coefficients[0] * sensorRMS*sensorRMS;
    return trueRMS;

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

// float ADSreads::readRealIns(int channel, float a, float b)
// {
//     float voltage = readInst(channel);
//     return (a * voltage + b);
// }