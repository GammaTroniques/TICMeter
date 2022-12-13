#include <HardwareSerial.h>
#include <linky.h>

Linky::Linky(HardwareSerial *toto)
{
    this->toto = toto;
}