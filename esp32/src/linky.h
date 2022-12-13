#ifndef Linky_H
#define Linky_H
#include <HardwareSerial.h>

#define MODE_HISTORIQUE 0
#define MODE_STANDARD 1

#define START_OF_FRAME 0x02 // The start of frame character
#define END_OF_FRAME 0x03   // The end of frame character

#define START_OF_GROUP 0x0A  // The start of group character
#define END_OF_GROUP 0x0D    // The end of group character
#define GROUP_SEPARATOR 0x20 // The group separator character

#define BUFFER_SIZE 400 // The size of the UART buffer
#define FRAME_SIZE 200  // The size of one frame buffer
#define GROUP_COUNT 10

typedef struct
{
    unsigned long ADCO = 0;   // The linky serial number
    char OPTARIF[5] = {0};    // The linky tarif option
    unsigned long ISOUSC = 0; // The linky max current
    unsigned long BASE = 0;   // The linky base index
    unsigned long HCHC = 0;   // The linky HC index
    unsigned long HCHP = 0;   // The linky HP index
    char PTEC[5] = {0};       // The linky current tarif
    unsigned long IINST = 0;  // The linky current current
    unsigned long IMAX = 0;   // The linky max current
    unsigned long PAPP = 0;   // The linky current power
    char HHPHC[4] = {0};      // The linky .......
    char MOTDETAT[7] = {0};
    unsigned long timestamp = 0;
} LinkyData;

class Linky
{

public:
    HardwareSerial *UART;                              // The UART port
    Linky(char mode, int RX, int TX); // Constructor
                                                       //
    char update();                                     // Update the data
    void print();                                      // Print the data
                                                       //
    LinkyData data;
    void begin(); // Begin the linky

private:
    char UARTmode = 0; // The mode of the linky
    char UARTRX = 0;   // The RX pin of the linky
    char UARTTX = 0;   // The TX pin of the linky

    char buffer[BUFFER_SIZE] = {0};                                      // The UART buffer
    void arrayCopy(char *source, char *destination, int start, int end); // Copy an array
    void read();                                                         // Read the UART buffer
    char decode();                                                       // Decode the frame
    char checksum(char label[], char data[]);                            // Check the checksum
};

#endif