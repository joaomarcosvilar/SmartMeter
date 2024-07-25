#ifndef LORAMESH_H
#define LORAMESH_H

#include <Arduino.h>

class LoRaMesh
{
public:
    char LerSerial();
    char TrataRX();
    uint16_t CalcCRC();
private:
};

#endif