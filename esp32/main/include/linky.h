#ifndef Linky_H
#define Linky_H
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "string.h"

#define MODE_HISTORIQUE 0
#define MODE_STANDARD 1

#define START_OF_FRAME 0x02 // The start of frame character
#define END_OF_FRAME 0x03   // The end of frame character

#define START_OF_GROUP 0x0A  // The start of group character
#define END_OF_GROUP 0x0D    // The end of group character
#define GROUP_SEPARATOR 0x20 // The group separator character

#define RX_BUF_SIZE 1024 // The size of the UART buffer
#define BUFFER_SIZE 1024 // The size of the UART buffer
#define FRAME_SIZE 200   // The size of one frame buffer
#define GROUP_COUNT 10

#define LINKY_TAG "Linky"

typedef struct
{
    uint32_t ADCO = 0;     // The linky serial number
    char OPTARIF[5] = {0}; // The linky tarif option
    uint32_t ISOUSC = 0;   // The linky max current
    uint32_t BASE = 0;     // The linky base index
    uint32_t HCHC = 0;     // The linky HC index
    uint32_t HCHP = 0;     // The linky HP index
    char PTEC[5] = {0};    // The linky current tarif
    uint32_t IINST = 0;    // The linky current current
    uint32_t IMAX = 0;     // The linky max current
    uint32_t PAPP = 0;     // The linky current power
    char HHPHC[4] = {0};   // The linky .......
    char MOTDETAT[7] = {0};
    uint32_t timestamp = 0;
} LinkyData;

class Linky
{

public:
    Linky(char mode, int RX, int TX); // Constructor
                                      //
    char update();                    // Update the data
    void print();                     // Print the data
                                      //
    LinkyData data;
    void begin(); // Begin the linky
    // void rx_task(void *arg);

    uint16_t index = 0;             // The index of the UART buffer
    char buffer[BUFFER_SIZE] = {0}; // The UART buffer
private:
    char UARTmode = 0; // The mode of the linky
    char UARTRX = 0;   // The RX pin of the linky
    char UARTTX = 0;   // The TX pin of the linky

    void read();                              // Read the UART buffer
    char decode();                            // Decode the frame
    char checksum(char label[], char data[]); // Check the checksum
};

#endif