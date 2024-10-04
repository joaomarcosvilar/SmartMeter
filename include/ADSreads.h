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
    float readRMS(int channel, float coefficients[]);
    void IRAM_ATTR onNewDataReady(); // Função de Interrupção do pino ALRT
    // float readADC(int channel);
    // float readRealIns(int channel, float a, float b);

private:
    int READY_PIN;                           // Pino ALRT do ADS1115
    uint16_t dataRate = RATE_ADS1115_860SPS; // 860 amostras por segundo
    int samples = 860;                       // Quant de amostras para cálculos das funções

    volatile bool newData = false;

    static ADSreads *instance; // Ponteiro estático para a instância
    uint8_t adress;

    uint16_t muxConfig;
    uint16_t translateMuxconfig(int channel);
    void setChannel(int channel);
    int lastChannel = -1;

    bool connectVerify(int channel);
    bool avaliable[4] = {true, true, true};
    float toleranceVoltage = 0.05; // Teste para identificar se o sensor está conectado e funcionando corretamente
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
        Serial.print("ADS initialized: ");
        Serial.println(adress);
    }

    ads.setDataRate(dataRate);

    pinMode(READY_PIN, INPUT_PULLUP);

    Serial.println("Verificando conexao...");
    for (int i = 0; i < 3; i++)
    {
        avaliable[i] = connectVerify(i);
        if(!avaliable[i])
            Serial.println("Sensor nao conectado ao canal " + String(i));
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Função auxiliar para os alertas de leituras do canal.
void IRAM_ATTR ADSreads::onNewDataReady()
{
    newData = true;
}

// Função auxiliar para mudança de canal para captura de dados.
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

// Muda qual o canal é setado para a leitura dos valores, configurando para o modo CONTINUOS.
void ADSreads::setChannel(int channel)
{
    if (channel != lastChannel)
    {
        muxConfig = translateMuxconfig(channel);
        ads.startADCReading(muxConfig, /*continuous=*/true);
        lastChannel = channel;
    }
}

// Leitura da frequência do sinal do canal selecionado.
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

// Leitura dos valores instantâneos do canal.
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

// Cálculo e conversão do sinal RMS do canal.
float ADSreads::readRMS(int channel, float coefficients[])
{
    int DEGREE = 3; // Grau do polinimio de calibração + 1

    // Verifica se todos os coeficientes estão no valor padrão 0, sendo necessário a calibração.
    int index = 0;
    for (int i = 0; i < DEGREE; i++)
    {
        if (coefficients[i] == 0)
            index++;
    }
    if (index == DEGREE)
        return 0;

    float sum = 0;
    for (int i = 0; i < samples; i++)
    {
        float instValue = readInst(channel);
        sum += instValue * instValue;
    }
    float sensorRMS = sqrt(sum / samples);
    // Serial.println(sensorRMS,6);

    float trueRMS = 0;

    if (coefficients[2] == 0.0) // Para a regressão linear da corrente (Apresentou erro menor +-0.1A)(Corrente)
    {
        trueRMS = coefficients[1] + coefficients[0] * sensorRMS;
    }
    else // Usando polinomio de grau 2 (Tensão)
    {
        trueRMS = coefficients[2] + coefficients[1] * sensorRMS + coefficients[0] * sensorRMS * sensorRMS;
    }

    return trueRMS;
}

// float ADSreads::readADC(int channel)
// {
//     if (avaliable[channel])
//     {
//         setChannel(channel);
//         unsigned long startTime = millis();

//         while (millis() - startTime < 500)
//         {
//             if (newData)
//             {
//                 newData = false;
//                 return (ads.getLastConversionResults());
//             }
//         }
//         Serial.println("ADC error in readADC");
//         return 0;
//     }
//     else
//     {
//         Serial.println("Sensor não reconhecido!");
//         return 0;
//     }
// }

// Cálculo e conversão do sinal RMS do canal.
bool ADSreads::connectVerify(int channel)
{
    // Verifica se todos os coeficientes estão no valor padrão 0, sendo necessário a calibração.
    float min = 5.0;
    float max = 0.0;

    for (int i = 0; i < samples; i++)
    {
        int current = readInst(channel);

        if (current > max)
        {
            max = current;
            continue;
        }
        if (current < min)
        {
            min = current;
            continue;
        }
    }

    if ((max - min) < toleranceVoltage)
        return false;
    else
        return true;
}
