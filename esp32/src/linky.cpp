#include <linky.h>
/**
 * @brief Linky constructor
 *
 * @param mode MODE_STANDARD or MODE_HISTORIQUE
 * @param RX RX pin number for the UART
 * @param TX TX pin number for the UART (not used)
 */
Linky::Linky(char mode, int RX, int TX)
{

    UARTmode = mode;
    UARTRX = RX;
    UARTTX = TX;
}

/**
 * @brief Read the data from the UART and store it in the buffer
 *
 */
void Linky::read()
{
    char c = 0;                                        // store the current read character
    unsigned int index = 0;                            // store the current index of the buffer
    memset(buffer, 0, sizeof buffer);                  // clear the buffer
    while (Serial2.available() && index < BUFFER_SIZE) // while there is a character available and the buffer is not full
    {
        c = Serial2.read();  // read the character from the UART
        buffer[index++] = c; // store the character in the buffer and increment the index
    }
}

/**
 * @brief Decode the data from the buffer and store it in variables
 *
 * @return 0 if an error occured, 1 if the data is valid
 */
char Linky::decode()
{
    //----------------------------------------------------------
    // Clear the previous data
    //----------------------------------------------------------
    memset(&data, 0, sizeof(data));

    //----------------------------------------------------------
    // Firt step: find the start and end of the frame
    //----------------------------------------------------------
    unsigned int startOfFrame = UINT_MAX; // store the index of the start frame
    unsigned int endOfFrame = UINT_MAX;   // store the index of the end frame
    for (int i = 0; i < BUFFER_SIZE; i++) // for each character in the buffer
    {
        if (buffer[i] == START_OF_FRAME) // if the character is a start of frame
        {
            startOfFrame = i; // store the index
        }
        else if (buffer[i] == END_OF_FRAME && i > startOfFrame) // if the character is an end of frame and an start of frame has been found
        {
            endOfFrame = i; // store the index
            break;          // stop the loop
        }
    }

    if (endOfFrame == UINT_MAX || startOfFrame == UINT_MAX || startOfFrame > endOfFrame) // if the start or the end of the frame are not found or if the start is after the end
    {
        // ERROR
        return 0; // exit the function
    }

    char frame[200] = {0};                                           // store the frame
    memcpy(frame, buffer + startOfFrame, endOfFrame - startOfFrame); // copy only one frame from the buffer

    //-------------------------------------
    // Second step: Find goups of data in the frame
    //-------------------------------------
    unsigned int startOfGroup[GROUP_COUNT] = {UINT_MAX}; // store starts index of each group
    unsigned int endOfGroup[GROUP_COUNT] = {UINT_MAX};   // store ends index of each group
    unsigned int startOfGroupIndex = 0;                  // store the current index of starts of group array
    unsigned int endOfGroupIndex = 0;                    // store the current index of ends of group array

    for (unsigned int i = 0; i < FRAME_SIZE; i++) // for each character in the frame
    {
        switch (frame[i])
        {
        case START_OF_GROUP:                       // if the character is a start of group
            startOfGroup[startOfGroupIndex++] = i; // store the index and increment it
            break;                                 //
        case END_OF_GROUP:                         // if the character is a end of group
            endOfGroup[endOfGroupIndex++] = i;     // store the index and increment it
            break;
        default:
            break;
        }
    }

    if (startOfGroup[0] == UINT_MAX || endOfGroup[0] == UINT_MAX) // if not group found (keep the UINT_MAX value)
    {
        Serial.println("No group found");
        return 0; // exit the function (no group found)
    }

    if (startOfGroupIndex != endOfGroupIndex) // if the number of starts is not equal to the number of ends: Error
    {
        // error: number of start and end frames are not equal
        Serial.print("error: number of start and end frames are not equal:");
        Serial.print(startOfGroupIndex);
        Serial.print(" ");
        Serial.println(endOfGroupIndex);
        return 0;
    }

    //------------------------------------------
    // Third step: Find fields in each group
    //------------------------------------------
    for (int i = 0; i < startOfGroupIndex; i++) // for each group
    {
        unsigned int separatorIndex[startOfGroupIndex + 1] = {0};    // store index of separator
        for (int j = startOfGroup[i], s = 0; j < endOfGroup[i]; j++) // for each character in group
        {
            if (frame[j] == GROUP_SEPARATOR) // if the character is a separator
            {
                separatorIndex[s++] = j; // store the index of the separator
            }
        }

        char label[10] = {0};   // store the label as a string
        char value[20] = {0};   // store the data as a string
        char checksum[5] = {0}; // store the checksum as a string

        //-----------------------------------------------------------------------------------------------------------------replace to MEMCOPY
        memcpy(label, frame + startOfGroup[i] + 1, separatorIndex[0] - startOfGroup[i] - 1);     // copy the label from the group
        memcpy(value, frame + separatorIndex[0] + 1, separatorIndex[1] - separatorIndex[0] - 1); // copy the data from the group
        memcpy(checksum, frame + separatorIndex[1] + 1, endOfGroup[i] - separatorIndex[1] - 1);  // copy the checksum from the group

        if (this->checksum(label, value) != checksum[0]) // check the checksum with the
        {
            // error: checksum is not correct, skip the field
            // Serial.println("ERROR: Checksum KO");
        }
        else
        {
            //------------------------------------------------------------
            // Fourth step: Copy values from each field to the variables
            //------------------------------------------------------------
            if (strcmp(label, "ADCO") == 0)
            {
                data.ADCO = strtoul(value, NULL, 10);
            }
            else if (strcmp(label, "OPTARIF") == 0)
            {
                strcpy(data.OPTARIF, value);
            }
            else if (strcmp(label, "ISOUSC") == 0)
            {
                data.ISOUSC = strtoul(value, NULL, 10);
            }
            else if (strcmp(label, "BASE") == 0)
            {
                data.BASE = strtoul(value, NULL, 10);
            }
            else if (strcmp(label, "HCHC") == 0)
            {
                data.HCHC = strtoul(value, NULL, 10);
            }
            else if (strcmp(label, "HCHP") == 0)
            {
                data.HCHP = strtoul(value, NULL, 10);
            }
            else if (strcmp(label, "PTEC") == 0)
            {
                strcpy(data.PTEC, value);
            }
            else if (strcmp(label, "IINST") == 0)
            {
                data.IINST = strtoul(value, NULL, 10);
            }
            else if (strcmp(label, "IMAX") == 0)
            {
                data.IMAX = strtoul(value, NULL, 10);
            }
            else if (strcmp(label, "PAPP") == 0)
            {
                data.PAPP = strtoul(value, NULL, 10);
            }
            else if (strcmp(label, "HHPHC") == 0)
            {
                strcpy(data.HHPHC, value);
            }
            else if (strcmp(label, "MOTDETAT") == 0)
            {
                strcpy(data.MOTDETAT, value);
            }
        }
    }
    return 1;
}

/**
 * @brief Update the data from the Linky
 * Read the UART and decode the frame
 *
 * @return char 1 if success, 0 if error
 */
char Linky::update()
{
    read();       // read the UART
    if (decode()) // decode the frame
    {
        return 1;
    }
    return 0;
}

/**
 * @brief Print the data
 *
 */
void Linky::print()
{
    Serial.print("ADCO: ");
    Serial.println(data.ADCO);
    Serial.print("OPTARIF: ");
    Serial.println(data.OPTARIF);
    Serial.print("ISOUSC: ");
    Serial.println(data.ISOUSC);
    Serial.print("BASE: ");
    Serial.println(data.BASE);
    Serial.print("HCHC: ");
    Serial.println(data.HCHC);
    Serial.print("HCHP: ");
    Serial.println(data.HCHP);
    Serial.print("PTEC: ");
    Serial.println(data.PTEC);
    Serial.print("IINST: ");
    Serial.println(data.IINST);
    Serial.print("IMAX: ");
    Serial.println(data.IMAX);
    Serial.print("PAPP: ");
    Serial.println(data.PAPP);
    Serial.print("HHPHC: ");
    Serial.println(data.HHPHC);
    Serial.print("MOTDETAT: ");
    Serial.println(data.MOTDETAT);
    Serial.println("----------------");
}

/**
 * @brief Calculate the checksum
 *
 * @param label name of the field
 * @param data value of the field
 * @return return the character of the checksum
 */
char Linky::checksum(char label[], char data[])
{
    int S1 = 0;                             // sum of the ASCII codes of the characters in the label
    for (int i = 0; i < strlen(label); i++) // for each character in the label
    {                                       //
        S1 += label[i];                     // add the ASCII code of the label character to the sum
    }                                       //
    S1 += GROUP_SEPARATOR;                  // add the ASCII code of the separator to the sum
    for (int i = 0; i < strlen(data); i++)  // for each character in the data
    {                                       //
        S1 += data[i];                      // add the ASCII code of the data character to the sum
    }                                       //
    return (S1 & 0x3F) + 0x20;              // return the checksum
}

/**
 * @brief Start the serial communication
 *
 */
void Linky::begin()
{
    Serial2.end();                                   // stop the serial communication
    Serial2.begin(1200, SERIAL_7E1, UARTRX, UARTTX); // start the serial communication at 1200 bauds, 7E1, RX on pin RX, TX on pin TX
}