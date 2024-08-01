#ifndef LORAMESHLIB_H
#define LORAMESHLIB_H

#include <Arduino.h>
#include <LoRaMESH.h>

class LoRaEnd
{
public:
    LoRaEnd(int TXpin, int RXpin);
    void begin(int _baudrate);
    void sendMaster(String dados);

private:
    HardwareSerial *_serial;
    LoRaMESH lora;
    int _TXpin, _RXpin;
    uint8_t command;
    int id;
};

#endif

LoRaEnd::LoRaEnd(int TXpin, int RXpin)
    : _TXpin(TXpin), _RXpin(RXpin), _serial(&Serial2), lora(&Serial2)
{
}

void LoRaEnd::begin(int _baudrate)
{
    _serial->begin(_baudrate, SERIAL_8N1, _RXpin, _TXpin);
    lora.begin(true);
    Serial.println("LocalID: " + String(lora.localId));
    Serial.println("UniqueID: " + String(lora.localUniqueId));
    Serial.println("Pass <= 65535: " + String(lora.registered_password));
}

void LoRaEnd::sendMaster(String dados)
{
    uint8_t payload[dados.length()];
    dados.getBytes(payload, dados.length() + 1);
    uint8_t payloadSize = dados.length() + 1;
    command = 0xD6; ;// comando errado
    id = 0;
    if (lora.PrepareFrameCommand(id , command, payload, payloadSize))
    {
        if (lora.SendPacket())
        {
            Serial.println("Pacote enviado com sucesso!");
        }
        else
        {
            Serial.println("Falha ao enviar o pacote!");
        }
    }
    else
    {
        Serial.println("Falha ao preparar o pacote!");
    }
}
