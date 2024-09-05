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
    lora.begin(true);

    if (lora.localId != data["LoRaMESH"]["ID"])
    {
        lora.setnetworkId(data["LoRaMESH"]["ID"]);
        lora.setpassword(data["LoRaMESH"]["Password"]);

        // TODO: traduzir os dados do JSON para cada config
        // Config abaixa usada por padrão
        lora.config_bps(BW125, SF_LoRa_7, CR4_5);
        lora.config_class(LoRa_CLASS_C, LoRa_WINDOW_15s);
        

    }

    // Serial.println("LocalID: " + String(lora.localId));
    // Serial.println("UniqueID: " + String(lora.localUniqueId));
    // Serial.println("Pass <= 65535: " + String(lora.registered_password));
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
            // Serial.println("Pacote enviado com sucesso!");
        }
        else
        {
            // Serial.println("Falha ao enviar o pacote!");
        }
    }
    else
    {
        // Serial.println("Falha ao preparar o pacote!");
    }
}
