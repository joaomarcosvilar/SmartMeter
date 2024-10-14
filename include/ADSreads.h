#ifndef ADSREADS_H
#define ADSREADS_H

#include <Adafruit_ADS1X15.h>
#include <Arduino.h>
#include <ArduinoJson.h>

#define SAMPLES 860

class ADSreads
{
public:
    ADSreads(uint8_t _address);              // Construtor com endereço e offset
    void begin();                            // Inicializa o ADS
    void readADC(uint8_t channel);           // Captura leituras
    float mean(uint8_t channel);             // Calcula média
    float rmsSensor(uint8_t channel);        // Calcula RMS
    JsonDocument autocalib(uint8_t channel); // Realiza autocalibração
    float frequencia(uint8_t channel);

private:
    void clear();
    Adafruit_ADS1115 ads;      // Objeto para ADS1115
    int16_t offsetADC[3];      // Offset ADC
    int16_t leituras[SAMPLES]; // Array de leituras
    uint8_t address;

    bool connected[3] = {true};
};

#endif // ADSreads_H

ADSreads::ADSreads(uint8_t _address) : address(_address)
{
}

void ADSreads::begin()
{
    ads.setDataRate(RATE_ADS1115_860SPS);

    if (!ads.begin(address))
    {
        Serial.println("Failed to initialize ADS.");
        while (1)
            ;
    }
    clear();

    for (int i = 0; i < 3; i++)
    {
        offsetADC[i] = (int16_t)mean(i);
    }
    clear();

    // ANELL para Nordeste: 59Hz a 61hz, considerando intervalor entre 55 e 65
    // for (int i = 0; i < 3; i++)
    // {
    //     float freq = frequencia(i);
    //     if ((freq < 65.0) && (freq > 55.0))
    //         connected[i] = true;
    //     else
    //         Serial.println("Sensor não conectado ao canal: " + String(i));
    // }
}

void ADSreads::readADC(uint8_t channel)
{
    if (connected[channel])
    {
        for (int i = 0; i < 860; i++)
        {
            leituras[i] = ads.readADC_SingleEnded(0) - offsetADC[channel];
        }
    }
}

float ADSreads::mean(uint8_t channel)
{
    if (!connected[channel])
        return 0;

    float sum = 0;
    for (int i = 0; i < 860; i++)
    {
        leituras[i] = ads.readADC_SingleEnded(channel);
        sum += leituras[i];
    }
    return sum / 860; // Retorna a média
}

float ADSreads::rmsSensor(uint8_t channel)
{
    if (!connected[channel])
        return 0;

    readADC(channel);
    float sum = 0;
    for (int i = 0; i < 860; i++)
    {
        sum += leituras[i] * leituras[i];
    }
    float rms = sqrt(sum / 860);
    clear(); // Limpa as leituras após cálculo
    return rms;
}

void ADSreads::clear()
{
    for (int i = 0; i < SAMPLES; i++)
    {
        leituras[i] = 0;
    }
}

JsonDocument ADSreads::autocalib(uint8_t channel)
{
    JsonDocument coef;

    if (!connected[channel])
    {
        Serial.println("Sensor não conectado ao canal: " + String(channel) + "Verifique a conexão do sensor ao Smartmeter.");
        coef["a"] = 0.0;
        coef["b"] = 0.0;
        return coef;
    }

    Serial.println("Desligue as cargas, confirme com qualquer digito.");
    while (true)
    {
        if (Serial.available() > 0)
        {
            String str = Serial.readString();
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    offsetADC[channel] = (int16_t)mean(channel);
    Serial.println(/*"offsetADC" + String(channel) + ":" + String(offsetADC[channel]) + */"Iniciando autocalibração.");
    readADC(channel);
    float zeroRMSadc = rmsSensor(channel);
    Serial.println(/*"zeroRMSadc: " + String(zeroRMSadc) + */"Ligue a carga, confirme com o valor do multímetro.");
    String inputString = "";
    float maxRMS = 0.0;
    while (true)
    {
        if (Serial.available() > 0)
        {
            inputString = Serial.readString();
            Serial.println(inputString);
            maxRMS = inputString.toFloat();
            Serial.println("maxRMS: " + String(maxRMS));
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    float maxRMSadc = rmsSensor(channel);

    // Cálculo dos coeficientes
    float a = maxRMS / (maxRMSadc - zeroRMSadc);
    float b = -a * zeroRMSadc;
    coef["a"] = a;
    coef["b"] = b;
    Serial.println("Coeficientes:\n\ta: " + String(a) + "\tb: " + String(b));

    serializeJson(coef, Serial);
    return coef;
}

float ADSreads::frequencia(uint8_t channel)
{
    // Le a frequencia do sinal
    int zeroCrossings = 0;
    int index = 0;
    uint16_t ADC = 0, lastADC = 0;

    index = 0;
    unsigned long startMillis = millis();
    for (int i = 0; i < SAMPLES; i++)
    {
        ADC = ads.readADC_SingleEnded(channel);
        if ((lastADC > offsetADC[channel] && ADC <= offsetADC[channel]) || (lastADC < offsetADC[channel] && ADC >= offsetADC[channel]))
        {
            zeroCrossings++;
        }
        lastADC = ADC;
    }

    float freq = (zeroCrossings / 2.0) / ((millis() - startMillis) / 1000.0);

    return freq;
}
