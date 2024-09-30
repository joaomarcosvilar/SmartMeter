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
};

#endif

LoRaEnd::LoRaEnd(int TXpin, int RXpin)
    : _TXpin(TXpin), _RXpin(RXpin), _serial(&Serial2), lora(&Serial2)
{
}

void LoRaEnd::begin(JsonDocument _data)
{
    data.set(_data);
    _serial->begin(data["LoRaMESH"]["Baudrate"], SERIAL_8N1, _RXpin, _TXpin);

    if (data["debug"])
        lora.begin(true);
    else
        lora.begin();

    if (lora.localId != data["LoRaMESH"]["ID"])
    {
        lora.setnetworkId(data["LoRaMESH"]["ID"]);
        lora.setpassword(data["LoRaMESH"]["Password"]);

        // TODO: traduzir os dados do ChangeInterface para uint8_t
        // Config abaixa usada por padrão
        lora.config_bps(data["LoRaMESH"]["BD"], data["LoRaMESH"]["SF"], data["LoRaMESH"]["CRate"]);
        lora.config_class(data["LoRaMESH"]["Class"], data["LoRaMESH"]["Window"]);
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
            Serial.println("----------------Enviado por LoRaMESH----------------");
            Serial.println("\tTamanho: " + String(lora.frame.size));

            // TODO: não funcionou
            // unsigned long starttime = millis();
            // while (1)
            // {
            //     if ((millis() - starttime) > 2000)
            //     {
            //         break;
            //     }
            //     if (_serial->available() > 1)
            //     {
                    
            //         String c = _serial->readString();
            //         Serial.print("c: ");
            //         for (int i = 0; i < c.length(); i++)
            //             Serial.print(c[i]);
            //         Serial.println();

            //         String ida;
            //         bool falg = false;
            //         for (int i = 0; i < c.length(); i++)
            //         {
            //             if (c[i] == '{')
            //                 falg = true;
            //             if (falg)
            //             {
            //                 if (c[i] == ',')
            //                 {
            //                     ida[i] = '\0';
            //                     break;
            //                 }
            //                 ida[i] = c[i];
            //             }
            //         }

            //         Serial.print("ida: ");
            //         for (int i = 0; i < ida.length(); i++)
            //             Serial.print(ida[i]);
            //         Serial.println();

            //         break;
            //     }
            //     vTaskDelay(pdMS_TO_TICKS(100));
            // }
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