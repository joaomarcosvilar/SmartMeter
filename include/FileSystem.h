#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <Arduino.h>
#include "FS.h"
#include "SPIFFS.h"
#include <ArduinoJson.h>

#define FORMAT_SPIFFS_IF_FAILED true

class MySPIFFS
{
public:
    MySPIFFS();
    void begin();
    void list(String path);
    void insCoef(String _sensor, int channel, float coefficients[]);
    float getCoef(String _sensor, int channel, int coefficient);
    void format();

private:
    void initCalibration();
    String sensor;
};
#endif

#define DEFAULT 0.0
#define DEGREE 3 // Grau do polinimio de calibração + 1

MySPIFFS::MySPIFFS() {}

void MySPIFFS::begin()
{
    if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED))
    {
        // Serial.println("SPIFFS Mount Failed");
    }
    if (!SPIFFS.exists("/calibration.txt"))
    {
        initCalibration();
    }
    // else
    // {
    // list("/calibration.txt");
    // }
}


/*
    Caso não exista o arquivo calibration.py, vai criar no formato JSON com valores
    padrões de DEFAULT.
*/
void MySPIFFS::initCalibration()
{
    DynamicJsonDocument data(384);

    for (int i = 0; i < 2; i++)
    {
        if (i == 0)
        {
            sensor = "V";
        }
        else
        {
            sensor = "I";
        }
        for (int j = 0; j < 3; j++)
        {
            for (int c = 0; c < DEGREE; c++)
            {
                data[sensor][j][c] = DEFAULT;
            }
        }
    }

    String str;
    serializeJson(data, str);
    File file = SPIFFS.open("/calibration.txt", FILE_WRITE);
    file.print(str);
    file.close();
}

/*
    Mostra o conteúdo do arquivo no SPFFIS do ESP. Argumento é o caminho e 
    o nome do arquivo desejado.
*/
void MySPIFFS::list(String path)
{
    File file = SPIFFS.open(path, FILE_READ);
    Serial.println(path);
    if (!file)
    {
        Serial.println("Error opening file for reading");
        return;
    }
    Serial.print("File content: ");
    Serial.println(file.readString());

    file.close();
}


/*
    Insere os coeficientes de calibração dos sensores.
*/
void MySPIFFS::insCoef(String _sensor, int channel, float coefficients[])
{
    File file = SPIFFS.open("/calibration.txt", FILE_READ);
    DynamicJsonDocument data(384);
    DeserializationError error = deserializeJson(data, file);
    // error can be used for debugging
    file.close();
    for (int i = 0; i < DEGREE; i++)
    {
        data[_sensor][channel][i] = coefficients[i];
    }

    file = SPIFFS.open("/calibration.txt", FILE_WRITE);
    String str;
    serializeJson(data, str);
    file.print(str);
    file.close();
}

/*
    Lê e retorna os coeficientes de calibração dos sensores.
*/
float MySPIFFS::getCoef(String _sensor, int channel, int coefficient)
{
    File file = SPIFFS.open("/calibration.txt", FILE_READ);
    DynamicJsonDocument data(384);
    DeserializationError error = deserializeJson(data, file);
    // error can be used for debugging
    file.close();
    // serializeJson(data,Serial);
    // Serial.println();
    float dado = data[_sensor][channel][coefficient];
    // Serial.println(dado,6);
    return dado;
}

/*
    Formata a memória do ESP32.
*/
void MySPIFFS::format()
{
    Serial.println("Erasing SPIFFS...");
    if (SPIFFS.format())
    {
        Serial.println("SPIFFS erased successfully.");
    }
    else
    {
        Serial.println("Error erasing SPIFFS.");
    }
}
