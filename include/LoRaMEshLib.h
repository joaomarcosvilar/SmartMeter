#ifndef LORAMESHLIB_H
#define LORAMESHLIB_H

#include <Arduino.h>
#include <LoRaMESH.h>

class LoRaEnd
{
public:
    LoRaEnd(int TXpin, int RXpin);
    void begin(JsonDocument data);
    void sendMaster(String dados);
    int idRead();
    void RSSI(int id);

private:
    HardwareSerial *_serial;
    LoRaMESH lora;
    int _TXpin, _RXpin;
    uint8_t command;
    int id;
    JsonDocument data;
    uint8_t parseString(String subdado);
};

#endif

LoRaEnd::LoRaEnd(int TXpin, int RXpin)
    : _TXpin(TXpin), _RXpin(RXpin), _serial(&Serial2), lora(&Serial2)
{
}

void LoRaEnd::begin(JsonDocument _data)
{
    data.set(_data);
    _serial->begin(data["loramesh"]["bd"], SERIAL_8N1, _RXpin, _TXpin);

    if (data["debug"])
        lora.begin(true);
    else
        lora.begin();

    if (lora.localId != data["loramesh"]["id"])
    {
        lora.setnetworkId(data["loramesh"]["id"]);
        lora.setpassword(data["loramesh"]["pwd"]);

        // TODO: traduzir os dados do ChangeInterface para uint8_t
        // Config abaixa usada por padrão
        uint8_t DB = parseString(data["loramesh"]["bw"]);
        uint8_t SF = parseString(data["loramesh"]["sf"]);
        uint8_t CRate = parseString(data["loramesh"]["crate"]);
        uint8_t Class = parseString(data["loramesh"]["class"]);
        uint8_t Window = parseString(data["loramesh"]["window"]);

        if (data["debug"])
        {
            Serial.println("DB: " + String(DB));
            Serial.println("SF: " + String(SF));
            Serial.println("CRate: " + String(CRate));
            Serial.println("Class: " + String(Class));
            Serial.println("Window: " + String(Window));
        }
        lora.config_bps(DB, SF, CRate);
        lora.config_class(Class, Window);
    }

    Serial.println("LocalID: " + String(lora.localId));
    Serial.println("UniqueID: " + String(lora.localUniqueId));
    Serial.println("Pass <= 65535: " + String(lora.registered_password));
    // rotina de indentificação de conexão
}

int LoRaEnd::idRead()
{
    return lora.localId;
}

void LoRaEnd::sendMaster(String dados)
{
    uint8_t payload[dados.length() + 1];
    dados.getBytes(payload, dados.length() + 1); // mensagem.toCharArray(payload, tamanho);
    uint8_t payloadSize = dados.length() + 1;
    command = 0x10; // Para enviar uma String deve-se usar os comandos de aplicação, ou seja, no byte[2] colocar um comando com valor maior que 12 e menor que 128
    id = 0;         // Para o mestre

    if (lora.PrepareFrameCommand(id, command, payload, payloadSize))
    {
        if (lora.SendPacket())
        {
            String confimation = "Enviado por LoRaMESH:\n\tTamanho: " + String(lora.frame.size);
            unsigned long starttime = millis();
            while (1)
            {
                    if ((millis() - starttime) > 2000)
                    {
                        Serial.println("Master não encontrado.");
                        break;
                    }
                if (_serial->available() > 1)
                {
                    String input = _serial->readString();
                    // Serial.println("input: " + input + " tamando : " + input.length());

                    String c = "";
                    int index = 0;
                    bool flag = false;
                    while (index < input.length())
                    {
                        if (input[index] == '{')
                            flag = true;

                        if (flag)
                        {
                            c += input[index];
                            if (input[index] == '}')
                            {
                                break;
                            }
                        }
                        index++;
                    }
                    // Serial.println("c:" + c);

                    int pos = c.indexOf('|');
                    int end = c.indexOf('}');

                    confimation += "\n\tRSSI:\n\t\tSlave -> Master: -" + c.substring(pos + 1, end) + "\n\t\tMaster -> Slave: -" + c.substring(1, pos);
                    Serial.println(confimation);

                    break;
                }
              
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }
        else
        {
            Serial.println("Falha ao enviar o pacote!");
        }
    }
    // else
    // {
    //     // Serial.println("Falha ao preparar o pacote!");
    // }
}

// Ainda falta aqui, só mestre pode fazer leitura
void LoRaEnd::RSSI(int id)
{
    if (lora.localId != 0)
    {
    }
    Serial.println("Only the master can read the RSSI.");
}

uint8_t LoRaEnd::parseString(String subdado)
{

    if (subdado.equals("BW125"))
        return BW125;
    if (subdado.equals("BW250"))
        return BW250;
    if (subdado.equals("BW500"))
        return BW500;
    if (subdado.equals("SF_LoRa_7"))
        return SF_LoRa_7;
    if (subdado.equals("SF_LoRa_8"))
        return SF_LoRa_8;
    if (subdado.equals("SF_LoRa_9"))
        return SF_LoRa_9;
    if (subdado.equals("SF_LoRa_10"))
        return SF_LoRa_10;
    if (subdado.equals("SF_LoRa_11"))
        return SF_LoRa_11;
    if (subdado.equals("SF_LoRa_12"))
        return SF_LoRa_12;
    if (subdado.equals("SF_FSK"))
        return SF_FSK;
    if (subdado.equals("CR4_5"))
        return CR4_5;
    if (subdado.equals("CR4_6"))
        return CR4_6;
    if (subdado.equals("CR4_7"))
        return CR4_7;
    if (subdado.equals("CR4_8"))
        return CR4_8;
    if (subdado.equals("LoRa_CLASS_A"))
        return LoRa_CLASS_A;
    if (subdado.equals("LoRa_CLASS_C"))
        return LoRa_CLASS_C;
    if (subdado.equals("LoRa_WINDOW_5s"))
        return LoRa_WINDOW_5s;
    if (subdado.equals("LoRa_WINDOW_10s"))
        return LoRa_WINDOW_10s;
    if (subdado.equals("LoRa_WINDOW_15s"))
        return LoRa_WINDOW_15s;
    else
        return 0;
}