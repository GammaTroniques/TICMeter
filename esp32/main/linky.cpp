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
 * @brief Start the serial communication
 *
 */
void Linky::begin()
{
    // start the serial communication at 1200 bauds, 7E1
    uart_config_t uart_config = {
        .baud_rate = 1200,
        .data_bits = UART_DATA_7_BITS,
        .parity = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE, 0, 0, NULL, 0); // set UART1 buffer size
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, UARTTX, UARTRX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

/**
 * @brief Read the data from the UART and store it in the buffer
 *
 */
void Linky::read()
{
    uart_flush(UART_NUM_1);                                               // clear the UART buffer
    uint32_t rxBytes = 0;                                                 // store the number of bytes read
    uint32_t timeout = (xTaskGetTickCount() * portTICK_PERIOD_MS) + 5000; // 5 seconds timeout
    memset(buffer, 0, sizeof buffer);                                     // clear the buffer
    do
    {
        rxBytes += uart_read_bytes(UART_NUM_1, buffer + rxBytes, (RX_BUF_SIZE - 1) - rxBytes, 500 / portTICK_PERIOD_MS);
        ESP_LOGI(LINKY_TAG, "Read %lu bytes, remaning:%ld", rxBytes, timeout - xTaskGetTickCount() * portTICK_PERIOD_MS);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    } while ((rxBytes < 256) && ((xTaskGetTickCount() * portTICK_PERIOD_MS) < timeout));
    // ESP_LOG_BUFFER_HEXDUMP(LINKY_TAG, buffer, rxBytes, ESP_LOG_INFO);
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
    data = {0}; // clear the data structure
    // memset(&data, 0, sizeof(data));
    // ESP_LOGI(LINKY_TAG, "data: \n%s\n-----------------------\n\n", buffer);
    //----------------------------------------------------------
    // Firt step: find the start and end of the frame
    //----------------------------------------------------------
    uint32_t startOfFrame = UINT_MAX;     // store the index of the start frame
    uint32_t endOfFrame = UINT_MAX;       // store the index of the end frame
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
        ESP_LOGI(LINKY_TAG, "ERROR: no frame found or end of frame before start of frame");
        index = 0; // clear the buffer
        return 0;  // exit the function
    }

    char frame[500] = {0};                                           // store the frame
    memcpy(frame, buffer + startOfFrame, endOfFrame - startOfFrame); // copy only one frame from the buffer
    index = 0;                                                       // clear the buffer
    // ESP_LOGI(LINKY_TAG, "START OF FRAME: %lu (%x)", startOfFrame, buffer[startOfFrame]);
    // ESP_LOGI(LINKY_TAG, "END OF FRAME: %lu (%x)", endOfFrame, buffer[endOfFrame]);
    // ESP_LOG_BUFFER_HEXDUMP(LINKY_TAG, frame, endOfFrame - startOfFrame, ESP_LOG_INFO);
    // ESP_LOGI(LINKY_TAG, "PROCESS FRAME: %s", frame);
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
        case START_OF_GROUP: // if the character is a start of group
            // ESP_LOGI(LINKY_TAG, "START OF GROUP: %u (%x) --> startOfGroupIndex: %u", i, frame[i], startOfGroupIndex);
            startOfGroup[startOfGroupIndex++] = i; // store the index and increment it
            break;                                 //
        case END_OF_GROUP:                         // if the character is a end of group
            // ESP_LOGI(LINKY_TAG, "END OF GROUP: %u (%x) --> endOfGroupIndex: %u", i, frame[i], endOfGroupIndex);
            endOfGroup[endOfGroupIndex++] = i; // store the index and increment it
            break;
        default:
            break;
        }
    }

    if (startOfGroup[0] == UINT_MAX || endOfGroup[0] == UINT_MAX) // if not group found (keep the UINT_MAX value)
    {
        ESP_LOGI(LINKY_TAG, "No group found");
        return 0; // exit the function (no group found)
    }

    if (startOfGroupIndex != endOfGroupIndex) // if the number of starts is not equal to the number of ends: Error
    {
        // error: number of start and end frames are not equal
        ESP_LOGI(LINKY_TAG, "error: number of start and end frames are not equal: %d %d", startOfGroupIndex, endOfGroupIndex);
        return 0;
    }

    // for (int i = 0; i < startOfGroupIndex; i++) // for each group
    // {
    //     ESP_LOGI(LINKY_TAG, "Group %d: %d - %d", i, startOfGroup[i], endOfGroup[i]);
    // }

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

        // ESP_LOGI(LINKY_TAG, "label: %s value: %s checksum: %s", label, value, checksum);

        if (this->checksum(label, value) != checksum[0]) // check the checksum with the
        {
            // error: checksum is not correct, skip the field
            continue;
            ESP_LOGI(LINKY_TAG, "ERROR: %s checksum is not correct (%c != %c)", label, this->checksum(label, value), checksum[0]);
        }
        else
        {
            //------------------------------------------------------------
            // Fourth step: Copy values from each field to the variables
            //------------------------------------------------------------
            if (strcmp(label, "ADCO") == 0)
            {
                strcpy(data.ADCO, value);
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
    ESP_LOGI(LINKY_TAG, "ADCO: %s", data.ADCO);
    ESP_LOGI(LINKY_TAG, "OPTARIF: %s", data.OPTARIF);
    ESP_LOGI(LINKY_TAG, "ISOUSC: %ld", data.ISOUSC);
    ESP_LOGI(LINKY_TAG, "BASE: %ld", data.BASE);
    ESP_LOGI(LINKY_TAG, "HCHC: %ld", data.HCHC);
    ESP_LOGI(LINKY_TAG, "HCHP: %ld", data.HCHP);
    ESP_LOGI(LINKY_TAG, "PTEC: %s", data.PTEC);
    ESP_LOGI(LINKY_TAG, "IINST: %ld", data.IINST);
    ESP_LOGI(LINKY_TAG, "IMAX: %ld", data.IMAX);
    ESP_LOGI(LINKY_TAG, "PAPP: %ld", data.PAPP);
    ESP_LOGI(LINKY_TAG, "HHPHC: %s", data.HHPHC);
    ESP_LOGI(LINKY_TAG, "MOTDETAT: %s", data.MOTDETAT);
    ESP_LOGI(LINKY_TAG, "----------------");
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
