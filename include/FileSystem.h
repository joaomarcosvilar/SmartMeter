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
    void insCoef(String _sensor, int channel, float a, float b);
    float getCoef(String _sensor, int channel, String coefType);
    void format();

private:
    void initCalibration();
    String sensor;
};
#endif

#define DEFAULT_A 1.0
#define DEFAULT_B 0.0


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
    else
    {
        // list("/calibration.txt");
    }
}

void MySPIFFS::initCalibration()
{
    DynamicJsonDocument data(384);

    // Serial.println("Initializing calibration.txt");

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
            data[sensor][j]["a"] = DEFAULT_A;
            data[sensor][j]["b"] = DEFAULT_B;
        }
    }

    String str;
    serializeJson(data, str);
    File file = SPIFFS.open("/calibration.txt", FILE_WRITE);
    file.print(str);
    file.close();
}

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

void MySPIFFS::insCoef(String _sensor, int channel, float a, float b)
{
    File file = SPIFFS.open("/calibration.txt", FILE_READ);
    DynamicJsonDocument data(384);
    DeserializationError error = deserializeJson(data, file);
    // error can be used for debugging
    file.close();

    data[_sensor][channel]["a"] = a;
    data[_sensor][channel]["b"] = b;

    file = SPIFFS.open("/calibration.txt", FILE_WRITE);
    String str;
    serializeJson(data, str);
    file.print(str);
    file.close();
}

float MySPIFFS::getCoef(String _sensor, int channel, String coefType)
{
    File file = SPIFFS.open("/calibration.txt", FILE_READ);
    DynamicJsonDocument data(384);
    DeserializationError error = deserializeJson(data, file);
    // error can be used for debugging
    file.close();

    return data[_sensor][channel][coefType];
}

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
