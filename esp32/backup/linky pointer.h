#ifndef Linky_H
#define Linky_H
#include <HardwareSerial.h>


class Linky
{

public:
    HardwareSerial* toto;
    Linky(HardwareSerial* toto);
};

#endif