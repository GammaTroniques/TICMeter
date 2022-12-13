#ifndef Linky_H
#define Linky_H
#include <HardwareSerial.h>

#define START_OF_TRANSMISSION 0x02
#define END_OF_TRANSMISSION 0x03

#define START_OF_FRAME 0x0A
#define END_OF_FRAME 0x0D
#define FRAME_SEPARATOR 0x20

#define BUFFER_SIZE 175
#define FRAME_COUNT 10


class Linky
{

public:
    HardwareSerial UART;
    Linky(HardwareSerial& UART);

    unsigned long ADCO = 0;
    char OPTARIF[5] = {0};
    unsigned long ISOUSC = 0;
    unsigned long BASE = 0;
    unsigned long HCHC = 0;
    unsigned long HCHP = 0;
    char PTEC[5] = {0};
    unsigned long IINST = 0;
    unsigned long IMAX = 0;
    unsigned long PAPP = 0;
    char HHPHC[4] = {0};
    char MOTDETAT[7] = {0};

    void waitStartOfFrame();
    void read();
    void decode();

private:
    int startOfFrame[FRAME_COUNT];
    int endOfFrame[FRAME_COUNT];
    char buffer[BUFFER_SIZE];
};

#endif