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
    JsonDocument begin();
    void list(String path);
    String read(String path);
    void insCoef(String _sensor, int channel, float coefficients[]);
    float getCoef(String _sensor, int channel, int coefficient);
    void format();
    void ChangeInterface();
    void ChangeInterface(String interface, bool status);
    void ChangeInterface(String interface, String dado, String subdado);
    void ChangeInterface(String interface, String dado, int subdado);
    void ChangeInterface(String interface, String dado, String subdado, bool status, int int_subdado);
    void readALL();

private:
    void initCalibration();
    void initInterface();
    String sensor;
};
#endif

#define DEFAULT_Coef 0.0

MySPIFFS::MySPIFFS() {}

JsonDocument MySPIFFS::begin()
{
    if (SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED))
    {
        Serial.println("SPIFFS Mounted");
    }
    if (!SPIFFS.exists("/calibration.txt"))
        initCalibration();

    if (!SPIFFS.exists("/interface.json"))
        initInterface();

    // Identificar qual a interface está ativa
    File file = SPIFFS.open("/interface.json", FILE_READ);
    JsonDocument data;
    deserializeJson(data, file);
    // if (data["debug"])
    //     serializeJson(data, Serial);
    file.close();
    return data;
}

/*
    Caso não exista o arquivo calibration.py, vai criar no formato .txt com valores
    padrões de DEFAULT_Coef.
*/
void MySPIFFS::initCalibration()
{
    JsonDocument data;

    sensor = "V";
    for (int j = 0; j < 3; j++)
    {
        data[sensor][j][0] = 0.0;
        data[sensor][j][1] = 0.0;
    }

    sensor = "I";
    for (int j = 0; j < 3; j++)
    {
        data[sensor][j][0] = 0.0;
        data[sensor][j][1] = 0.0;
    }

    String str;
    serializeJson(data, str);
    File file = SPIFFS.open("/calibration.txt", FILE_WRITE);
    file.print(str);
    file.close();
}

void MySPIFFS::initInterface()
{
    JsonDocument data;
    // Debug
    data["debug"] = true;

    // Tempo de envio
    data["t"] = 5000;

    // Dados referente à comunicação wifi
    data["wifi"]["status"] = false;
    data["wifi"]["ssid"] = "SSID";
    data["wifi"]["pwd"] = "PWD";
    data["wifi"]["name"] = "SmartMeter";
    data["wifi"]["serv"] = "a1o3x5gedhdznd-ats.iot.us-east-2.amazonaws.com";
    data["wifi"]["topic"] = "smartmeter/power";
    data["wifi"]["subtopic"] = "smartmeter/subpower";
    data["wifi"]["port"] = 8883;

    // loramesh: é a comunicação DEFAULT_Coef, precisa configurar o loramesh na inicialização
    data["loramesh"]["status"] = true;
    data["loramesh"]["id"] = -1;                    /* ID: 0 é mestre, e 1-2046 é escravo  OBS.: Não existe função para
                                                        identificar se existe outros ID idênticos. Cada ID é utilizado
                                                        identificar qual o dispositivo SmartMeter.*/
    data["loramesh"]["bw"] = "BW500";               // Bandwith: 125KHz, 250KHz e 500KHz
    data["loramesh"]["sf"] = "SF_LoRa_7";           // SpreadingFactor: 7 - 12
    data["loramesh"]["crate"] = "CR4_5";            // Coding Rate: 4/5,4/6,4/7,4/8
    data["loramesh"]["class"] = "LoRa_CLASS_C";     // Classe: A e C
    data["loramesh"]["window"] = "LoRa_WINDOW_15s"; // Janela: 5, 10 e 15s
    data["loramesh"]["pwd"] = 123;                  // Senha: <= 65535
    data["loramesh"]["bd"] = 9600;

    // pppOSClient
    data["ppp"]["status"] = false;
    data["ppp"]["modem"] = "M95";
    data["ppp"]["op"] = "";

    // Salva na file interface.json
    File file = SPIFFS.open("/interface.json", FILE_WRITE);
    serializeJson(data, file);
    file.close();
}

void MySPIFFS::ChangeInterface()
{
    File file = SPIFFS.open("/interface.json", FILE_READ);
    JsonDocument data;
    DeserializationError error = deserializeJson(data, file);
    file.close();

    data["debug"] = !data["debug"];

    file = SPIFFS.open("/interface.json", FILE_WRITE);
    serializeJson(data, file);
    file.close();

    serializeJson(data, Serial);

    ESP.restart();
}

void MySPIFFS::ChangeInterface(String interface, bool status)
{
    ChangeInterface(interface, "", "", status, 0);
}

void MySPIFFS::ChangeInterface(String interface, String dado, String subdado)
{
    ChangeInterface(interface, dado, subdado, NULL, 0);
}

void MySPIFFS::ChangeInterface(String interface, String dado, int subdado)
{
    ChangeInterface(interface, dado, "", NULL, subdado);
}

void MySPIFFS::ChangeInterface(String interface, String dado, String subdado, bool status, int int_subdado)
{
    // Serial.print(interface); Serial.println(status);
    File file = SPIFFS.open("/interface.json", FILE_READ);
    JsonDocument data;
    DeserializationError error = deserializeJson(data, file);

    file.close();

    if (interface.equals("timer"))
        data[interface] = int_subdado;

    if (int_subdado > 0)
        data[interface][dado] = int_subdado;
    else
        data[interface][dado] = subdado;
    if (dado.equals("status"))
    {
        if (status || !status)
        {
            if (data["wifi"]["status"])
                data["wifi"]["status"] = false;

            if (data["loramesh"]["status"])
                data["loramesh"]["status"] = false;

            if (data["ppp"]["status"])
                data["ppp"]["status"] = false;

            data[interface]["status"] = true;
        }
    }

    file = SPIFFS.open("/interface.json", FILE_WRITE);
    serializeJson(data, file);
    file.close();

    // serializeJson(data, Serial);

    // ESP.restart();
}

/*
    Retorna todo conteúdo da file
*/
String MySPIFFS::read(String path)
{
    String read;
    File file = SPIFFS.open(path, FILE_READ);
    read = file.readString();
    // Serial.println(read.length());
    return read;
}

/*
    Mostra o conteúdo do arquivo no SPFFIS do ESP. Argumento é o caminho e
    o nome do arquivo desejado.
*/
void MySPIFFS::list(String path)
{
    File file = SPIFFS.open(path, FILE_READ);
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
    JsonDocument data;
    DeserializationError error = deserializeJson(data, file);
    // error can be used for debugging
    file.close();
    for (int i = 0; i < 2; i++)
    {
        data[_sensor][channel][i] = coefficients[i];
    }

    file = SPIFFS.open("/calibration.txt", FILE_WRITE);
    String str;

    // Debug
    serializeJson(data, Serial);

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
    JsonDocument data;
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
        // if (!SPIFFS.exists("/calibration.txt"))
        // initCalibration();

        // if (!SPIFFS.exists("/interface.json"))
        // initInterface();
        ESP.restart();
    }
    else
    {
        Serial.println("Error erasing SPIFFS.");
    }
}

void MySPIFFS::readALL()
{
    File root = SPIFFS.open("/");
    File file = root.openNextFile(); // Relata o próximo arquivo do "diretório" e
    //                                    passa o retorno para a variável
    //                                    do tipo File.
    while (file)
    { // Enquanto houver arquivos no "diretório" que não foram vistos,
        //                executa o laço de repetição.
        Serial.print("  FILE : ");
        Serial.print(file.name()); // Imprime o nome do arquivo
        Serial.print("\tSIZE : ");
        Serial.println(file.size()); // Imprime o tamanho do arquivo
        file = root.openNextFile();  // Relata o próximo arquivo do diretório e
                                     //                              passa o retorno para a variável
                                     //                              do tipo File.
    }
}
