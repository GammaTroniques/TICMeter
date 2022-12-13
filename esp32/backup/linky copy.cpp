#include <HardwareSerial.h>
#include <linky.h>
#define START_OF_TRANSMISSION 0x02
#define END_OF_TRANSMISSION 0x03

#define START_OF_FRAME 0x0A
#define END_OF_FRAME 0x0D
#define FRAME_SEPARATOR 0x20

#define BUFFER_SIZE 175
#define FRAME_COUNT 10

Linky::Linky(HardwareSerial UART)
{
    this->UART = UART;
}


void Linky::waitStartOfFrame()
{
    char c = 0;
    while (c != START_OF_TRANSMISSION) // wait start of transmission
    {
        if (UART.available())
        {
            c = UART.read();
        }
    }
}

void Linky::read()
{
    char c = 0;
    unsigned int index = 0;
    while (c != END_OF_TRANSMISSION) // Read until end of frame
    {
        if (UART.available())
        {
            c = UART.read();     // Read a character
            buffer[index++] = c; // Add the character to the buffer
        }
    }
}

void Linky::decode()
{
    for (int i = 0, s = 0, e = 0; i < BUFFER_SIZE; i++)
    {
        if (buffer[i] == START_OF_FRAME)
        {
            startOfFrame[s++] = i;
        }
        if (buffer[i] == END_OF_FRAME)
        {
            endOfFrame[e++] = i;
        }
    }

    for (int i = 0; i < FRAME_COUNT; i++) // for each frame
    {
        char separatorIndex[10];                                     // store index of separator
        for (int j = startOfFrame[i], s = 0; j < endOfFrame[i]; j++) // for each character in frame
        {
            if (buffer[j] == FRAME_SEPARATOR)
            {
                separatorIndex[s++] = j;
            }
        }

        char label[10] = {0};
        char data[20] = {0};
        char checksum[5] = {0};

        arrayCopy(label, buffer, startOfFrame[i] + 1, separatorIndex[0]);
        arrayCopy(data, buffer, separatorIndex[0] + 1, separatorIndex[1]);
        arrayCopy(checksum, buffer, separatorIndex[1], endOfFrame[i]);

        // Serial.print("Label: ");
        // Serial.println(label);
        // Serial.print("Data: ");
        // Serial.println(data);
        // Serial.print("Checksum: ");
        // Serial.println(checksum);
        // Serial.println("----------------");

        Serial.print("Data: ");
        Serial.println(data);
        if (strcmp(label, "ADCO") == 0)
        {
            ADCO = strtoul(data, NULL, 10);
        }
        else if (strcmp(label, "OPTARIF") == 0)
        {
            strcpy(OPTARIF, data);
        }
        else if (strcmp(label, "ISOUSC") == 0)
        {
            ISOUSC = strtoul(data, NULL, 10);
        }
        else if (strcmp(label, "BASE") == 0)
        {
            BASE = strtoul(data, NULL, 10);
        }
        else if (strcmp(label, "HCHC") == 0)
        {
            HCHC = strtoul(data, NULL, 10);
        }
        else if (strcmp(label, "HCHP") == 0)
        {
            HCHP = strtoul(data, NULL, 10);
        }
        else if (strcmp(label, "PTEC") == 0)
        {
            strcpy(PTEC, data);
        }
        else if (strcmp(label, "IINST") == 0)
        {
            IINST = strtoul(data, NULL, 10);
        }
        else if (strcmp(label, "IMAX") == 0)
        {
            IMAX = strtoul(data, NULL, 10);
        }
        else if (strcmp(label, "PAPP") == 0)
        {
            PAPP = strtoul(data, NULL, 10);
        }
        else if (strcmp(label, "HHPHC") == 0)
        {
            strcpy(HHPHC, data);
        }
        else if (strcmp(label, "MOTDETAT") == 0)
        {
            strcpy(MOTDETAT, data);
        }
    }
}

void Linky::arrayCopy(char *dest, char *src, int start, int end)
{
    int j = 0;
    for (int i = start; i < end; i++)
    {
        if (src[i] > 127)
        {
            dest[j] = 0;
        }
        else
        {
            dest[j] = src[i];
        }
        j++;
    }
}
