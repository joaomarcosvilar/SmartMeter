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
    void IRAM_ATTR onNewDataReady(); // Função de Interrupção do pino ALRT
    float readADC(int channel);
    float readRealIns(int channel);

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
    int lastChannel = -1; // Inicialize como um valor inválido

    float previousVoltage;
    int zeroCrossings, contt = 0;
    unsigned long currentTime, startTime = 0;

    // implementar uma struct para melhorar a organização
    float voltage, frequency, samplesRMS[860]; // Modificar o samplesRMS para config do ADS, 860 <- RATE_ADS1115_860SPS
    int index = 0;
    bool calibrate[4] = {false, false, false, false}, avaliable[4] = {true, false, false, false}; //<-- Ajustar após testes para todos true
    float meterRMS[4], mean[4], a[4], b[4], a_RMS[4], b_RMS[4], inRMS[4], iRMS[4], voltMax = 0.0, voltMin = 5.0, sumRMS = 0, RMS;
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

    // ads.setGain(GAIN_TWOTHIRDS); // 2/3x gain +/- 6.144V  1 bit = 0.1875mV   <- fazendo ocorrer um overshoot na leitura
    ads.setDataRate(dataRate);

    pinMode(READY_PIN, INPUT_PULLUP);

    // for (int i = 0; i < 4; i++)
    // {
    //     if (!calibrate[i])
    //         calibration(i);
    // }
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
    if (avaliable[channel])
    {
        setChannel(channel);
        zeroCrossings = 0;
        index = 0;
        unsigned long startMillis = millis();

        while (index < (samples + 1))
        {
            voltage = readInst(channel);
            if ((previousVoltage > mean[channel] && voltage <= mean[channel]) || (previousVoltage < mean[channel] && voltage >= mean[channel]))
            // if ((previousVoltage > 2.5 && voltage <= 2.5) || (previousVoltage < 2.5 && voltage >= 2.5))
            {
                zeroCrossings++;
            }
            previousVoltage = voltage;
            index++;
        }

        frequency = (zeroCrossings / 2.0) / ((millis() - startMillis) / 1000.0); // fazer tratamento para quando for desligado
        // Serial.print(">Frequency");
        // Serial.print(channel);
        // Serial.print(" :");
        // Serial.println(frequency);
        return frequency;

        // condição de leitura diferente de 60Hz
    }
    else
    {
        Serial.println("Sensor não reconhecido!");
        return 0;
    }
}

float ADSreads::readInst(int channel)
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
                // Serial.println(ads.computeVolts(ads.getLastConversionResults()));
                return (ads.computeVolts(ads.getLastConversionResults()));
                // return (a[channel] * ads.computeVolts(ads.getLastConversionResults()) - b[channel]);
            }
        }
        Serial.println("ADC error in readInst");
        return 0;
    }
    else
    {
        Serial.println("Sensor não reconhecido!");
        return 0;
    }
}

// Aquisição de várias amostras do sinal, cálculo do quadrado de cada amostra,
// média desses valores quadrados e, finalmente, extração da raiz quadrada da média.
float ADSreads::readRMS(int channel)
{
    if (avaliable[channel])
    {
        index = 0;
        sumRMS = 0;
        while (index < (samples + 1))
        {
            voltage = readInst(channel);
            sumRMS += (a[channel] * voltage + b[channel]) * (a[channel] * voltage + b[channel]);
            index++;
        }
        // Serial.print(sumRMS);
        // Serial.print("/");
        // Serial.print(samples);
        // Serial.print(" = ");

        RMS = sumRMS / samples;
        RMS = sqrt(RMS);

        return RMS;
    }
    else
    {
        Serial.println("Sensor não reconhecido!");
        return 0;
    }
}

void ADSreads::calibration(int channel)
{
    if (avaliable[channel])
    {

        // Calibração INST
        //  não está funcionando
        String inputString = "";
        Serial.print("Insert RMS read in meter: ");
        while (true)
        {
            if (Serial.available() > 0)
            {
                inputString = Serial.readString(); // Lê o próximo caractere disponível
                if (inputString.length() > 0) // Certifica-se de que algo foi digitado antes de Enter
                {
                    {
                        meterRMS[channel] = inputString.toFloat();
                        if (meterRMS[channel] == 0.0) // Identificador de sensor acoplado manualmente
                        {
                            avaliable[channel] = false;
                            Serial.println("Valor RMS inserido é 0.0. Sensor não disponível.");
                            return;
                        }
                        break;
                    }
                }
            }
        }
        meterRMS[0] = 216.3; //<-- para testes enquanto resolve problema com serial.read
        sumRMS = 0;
        voltMin = 5.0;
        voltMax = 0.0;
        contt = 0;
        while (contt < (3 * samples + 1))
        {
            voltage = readInst(channel);
            sumRMS += voltage;
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

        // identificador de sensor acoplado automático com marge de leitura max e min de 0.1V
        if ((voltMax - voltMin) < 0.1)
        {
            avaliable[channel] = false;
            Serial.println("Sensor não reconhecido! Variação minima não alcançada.");
            return;
        }

        Serial.println(voltMax, 6);
        Serial.println(voltMin, 6);

        mean[channel] = sumRMS / contt;
        Serial.println(mean[channel], 6);

        float vPeakPeak = 2.0 * meterRMS[channel] * sqrt(2);
        a[channel] = vPeakPeak / (voltMax - voltMin);
        b[channel] = -(a[channel] * mean[channel]);

        calibrate[channel] = true;
        Serial.print("Calibrado: y = ");
        Serial.print(a[channel], 6);
        Serial.print("*x ");
        Serial.println(b[channel], 6);

        // Calibração RMS <--- precisa de diversas leituras do multímetro, é viável?

        // for (int i = 0; i < 4; i++)
        // {
        //     //Solicitar em cada leitura o valor RMS do sensor
        //     iRMS[i] = 214.6;
        //     sumRMS = 0;
        //     while (contt < (samples + 1))
        //     {
        //         voltage = readInst(channel);
        //         sumRMS += voltage*voltage;
        //         contt++;
        //     }
        //     sumRMS = sumRMS/samples;
        //     inRMS[i] = sqrt(sumRMS);
        // }
    }
    else
    {
        Serial.println("Sensor não reconhecido!");
        return;
    }
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

float ADSreads::readRealIns(int channel)
{
    voltage = readInst(channel);
    return (a[channel] * voltage + b[channel]);
}