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
    void ChangeInterface(String interface, String dado, String subdado, bool status);
    void readALL();

private:
    void initCalibration();
    void initInterface();
    String sensor;
};
#endif

#define DEFAULT_Coef 0.0
#define DEGREE 3 // Grau do polinimio de calibração + 1

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
    // else
    // {
    // list("/calibration.txt");
    // }

    // Identificar qual a interface está ativa
    File file = SPIFFS.open("/interface.json", FILE_READ);
    JsonDocument data;
    deserializeJson(data, file);
    // serializeJson(data, Serial);
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

    for (int i = 0; i < 2; i++)
    {
        if (i == 0)
        {
            sensor = "V";
            for (int j = 0; j < 3; j++)
            {
                data[sensor][j][0] = -9035.0;
                data[sensor][j][1] = 47880.0;
                data[sensor][j][2] = -63221.0;
            }

            // {"V":[[-9035,47880,-63221],[0,0,0],[0,0,0]],"I":[[1331.170044,-3398.409912,0],[0,0,0],[0,0,0]]}
        }
        else
        {
            sensor = "I";
            for (int j = 0; j < 3; j++)
            {
                data[sensor][j][0] = 1331.170044;
                data[sensor][j][1] = -3398.409912;
                data[sensor][j][2] = 0.0;
            }
        }
        // for (int j = 0; j < 3; j++)
        // {
        //     for (int c = 0; c < DEGREE; c++)
        //     {
        //         data[sensor][j][c] = DEFAULT_Coef;
        //     }
        // }
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
    data["TIMER"] = 5000;

    // Dados referente à comunicação WiFi
    data["WiFi"]["Status"] = false;
    data["WiFi"]["SSID"] = "FREUD_EXPLICA_2.4G";
    data["WiFi"]["Password"] = "02040608";
    data["WiFi"]["THINGNAME"] = "SmartMeter";
    data["WiFi"]["AWS_IOT_ENDPOINT"] = "a1o3x5gedhdznd-ats.iot.us-east-2.amazonaws.com";

    // LoRaMESH: é a comunicação DEFAULT_Coef, precisa configurar o LoRaMesh na inicialização
    data["LoRaMESH"]["Status"] = true;
    data["LoRaMESH"]["ID"] = 1;         /* ID: 0 é mestre, e 1-2046 é escravo  OBS.: Não existe função para
                                             indentificar se existe outros ID idênticos. Ideia é atribuir cada
                                             pelo número do dispositivo produzido.*/
    data["LoRaMESH"]["BD"] = 500;       // Bandwith: 125KHz, 250KHz e 500KHz
    data["LoRaMESH"]["SF"] = 7;         // SpreadingFactor: 7 - 12
    data["LoRaMESH"]["CRate"] = 5;      // Coding Rate: 4/5,4/6,4/7,4/8
    data["LoRaMESH"]["Class"] = "C";    // Classe: A e C
    data["LoRaMESH"]["Window"] = 15;    // Janela: 5, 10 e 15s
    data["LoRaMESH"]["Password"] = 123; // Senha: <= 65535
    data["LoRaMESH"]["Baudrate"] = 9600;

    // PPPOSClient
    data["PPP"]["Status"] = false;
    data["PPP"]["Modem"] = "M95";
    data["PPP"]["Op"] = "";

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
    ChangeInterface(interface, "", "", status);
}

void MySPIFFS::ChangeInterface(String interface, String dado, String subdado)
{
    ChangeInterface(interface, dado, subdado, NULL);
}

void MySPIFFS::ChangeInterface(String interface, String dado, String subdado, bool status)
{
    // Serial.print(interface); Serial.println(status);
    File file = SPIFFS.open("/interface.json", FILE_READ);
    JsonDocument data;
    DeserializationError error = deserializeJson(data, file);

    file.close();

    data[interface][dado] = subdado;

    if (status || !status)
    {
        if (data["WiFi"]["Status"])
            data["WiFi"]["Status"] = false;

        if (data["LoRaMESH"]["Status"])
            data["LoRaMESH"]["Status"] = false;

        if (data["PPP"]["Status"])
            data["PPP"]["Status"] = false;

        data[interface]["Status"] = true;
    }

    file = SPIFFS.open("/interface.json", FILE_WRITE);
    serializeJson(data, file);
    file.close();

    serializeJson(data, Serial);

    ESP.restart();
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
    for (int i = 0; i < DEGREE; i++)
    {
        data[_sensor][channel][i] = coefficients[i];
    }

    file = SPIFFS.open("/calibration.txt", FILE_WRITE);
    String str;
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
