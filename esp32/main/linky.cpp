#include <linky.h>

Linky linky(MODE_HISTORIQUE, 17, 16);

// clang-format off
struct LinkyGroup LinkyLabelList[] =
{   
    // label         data            type
    {"ADCO",        &linky.data.ADCO,     TYPE_STRING},
    {"OPTARIF",     &linky.data.OPTARIF,  TYPE_STRING},
    {"ISOUSC",      &linky.data.ISOUSC,   TYPE_UINT16},

    {"BASE",        &linky.data.BASE,     TYPE_UINT48},

    {"HCHC",        &linky.data.HCHC,     TYPE_UINT48},
    {"HCHP",        &linky.data.HCHP,     TYPE_UINT48},

    {"EJPHN",       &linky.data.EJPHN,    TYPE_UINT48},
    {"EJPHPM",      &linky.data.EJPHPM,   TYPE_UINT48},
    {"PEJP",        &linky.data.PEJP,     TYPE_UINT48},

    {"BBRHCJB",     &linky.data.BBRHCJB,  TYPE_UINT48},
    {"BBRHPJB",     &linky.data.BBRHPJB,  TYPE_UINT48},
    {"BBRHCJW",     &linky.data.BBRHCJW,  TYPE_UINT48},
    {"BBRHPJW",     &linky.data.BBRHPJW,  TYPE_UINT48},
    {"BBRHCJR",     &linky.data.BBRHCJR,  TYPE_UINT48},
    {"BBRHPJR",     &linky.data.BBRHPJR,  TYPE_UINT48},

    {"PTEC",        &linky.data.PTEC,     TYPE_STRING},
    {"DEMAIN",      &linky.data.DEMAIN,   TYPE_STRING},

    {"IINST",       &linky.data.IINST,    TYPE_UINT16},
    {"IINST1",      &linky.data.IINST1,   TYPE_UINT16},
    {"IINST2",      &linky.data.IINST2,   TYPE_UINT16},
    {"IINST3",      &linky.data.IINST3,   TYPE_UINT16},
    {"IMAX",        &linky.data.IMAX,     TYPE_UINT16},
    {"IMAX1",       &linky.data.IMAX1,    TYPE_UINT16},
    {"IMAX2",       &linky.data.IMAX2,    TYPE_UINT16},
    {"IMAX3",       &linky.data.IMAX3,    TYPE_UINT16},
    {"ADPS",        &linky.data.ADPS,     TYPE_UINT16},
    {"ADIR1",       &linky.data.ADIR1,    TYPE_UINT16},
    {"ADIR2",       &linky.data.ADIR2,    TYPE_UINT16},
    {"ADIR3",       &linky.data.ADIR3,    TYPE_UINT16},

    {"PAPP",        &linky.data.PAPP,     TYPE_UINT32},
    {"PAPP",        &linky.data.PMAX,     TYPE_UINT32},
    {"PAPP",        &linky.data.PPOT,     TYPE_UINT32},
    
    {"HHPHC",       &linky.data.HHPHC,    TYPE_UINT8},
    {"MOTDETAT",    &linky.data.MOTDETAT, TYPE_STRING},
};
// clang-format on
const int32_t LinkyLabelListSize = sizeof(LinkyLabelList) / sizeof(LinkyLabelList[0]);

/**
 * @brief Linky constructor
 *
 * @param mode MODE_STANDARD or MODE_HISTORIQUE
 * @param RX RX pin number for the UART
 * @param TX TX pin number for the UART (not used)
 */
Linky::Linky(LinkyMode mode, int RX, int TX)
{
    mode = mode;
    UARTRX = RX;
    UARTTX = TX;
    GROUP_SEPARATOR = 0x20 ? mode == MODE_HISTORIQUE : 0x09; // space or tab depending on the mode
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

    char frame[FRAME_SIZE] = {0};                                    // store the frame
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
        unsigned int separators[startOfGroupIndex + 1] = {0}; // store index of separators
        uint8_t separatorIndex = 0;                           // store the current index of separators array
        for (int j = startOfGroup[i]; j < endOfGroup[i]; j++) // for each character in group
        {
            if (frame[j] == GROUP_SEPARATOR) // if the character is a separator
            {
                separators[separatorIndex++] = j; // store the index of the separator
            }
        }

        char label[10] = {0};   // store the label as a string
        char value[100] = {0};  // store the data as a string
        char time[15] = {0};    // store the time as a string (H081225223518)
        char checksum[5] = {0}; // store the checksum as a string

        //-----------------------------------------------------------------------------------------------------------------replace to MEMCOPY
        memcpy(label, frame + startOfGroup[i] + 1, separators[0] - startOfGroup[i] - 1); // copy the label from the group
        memcpy(value, frame + separators[0] + 1, separators[1] - separators[0] - 1);     // copy the data from the group
        if (linky.mode == MODE_STANDARD && separatorIndex == 3)                          // if the mode is standard and the number of separators is 3
        {
            memcpy(time, frame + separators[1] + 1, separators[2] - separators[1] - 1);     // copy the time from the group
            memcpy(checksum, frame + separators[2] + 1, endOfGroup[i] - separators[2] - 1); // copy the checksum from the group
        }
        else
        {
            memcpy(checksum, frame + separators[1] + 1, endOfGroup[i] - separators[1] - 1); // copy the checksum from the group
        }
        // ESP_LOGI(LINKY_TAG, "label: %s value: %s checksum: %s", label, value, checksum);

        if (this->checksum(label, value, time) != checksum[0]) // check the checksum with the label, data and time
        {
            // error: checksum is not correct, skip the field
            continue;
            ESP_LOGI(LINKY_TAG, "ERROR: %s checksum is not correct (%c != %c)", label, this->checksum(label, value, time), checksum[0]);
        }
        else
        {
            //------------------------------------------------------------
            // Fourth step: Copy values from each field to the variables
            //------------------------------------------------------------
            for (uint32_t j = 0; j < LinkyLabelListSize; j++)
            {
                if (strcmp(LinkyLabelList[j].label, label) == 0)
                {
                    switch (LinkyLabelList[j].type)
                    {
                    case TYPE_STRING:
                        strcpy((char *)LinkyLabelList[j].data, value);
                        break;
                    case TYPE_UINT8:
                        *(uint8_t *)LinkyLabelList[j].data = strtoul(value, NULL, 10);
                        break;
                    case TYPE_UINT16:
                        *(uint16_t *)LinkyLabelList[j].data = strtoul(value, NULL, 10);
                        break;
                    case TYPE_UINT32:
                        *(uint32_t *)LinkyLabelList[j].data = strtoul(value, NULL, 10);
                        break;
                    case TYPE_UINT48:
                        *(uint64_t *)LinkyLabelList[j].data = strtoull(value, NULL, 10);
                        break;
                    default:
                        break;
                    }
                }
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
    for (uint32_t i = 0; i < LinkyLabelListSize; i++)
    {
        switch (LinkyLabelList[i].type)
        {
        case TYPE_STRING:
            ESP_LOGI(LINKY_TAG, "%s: %s", LinkyLabelList[i].label, (char *)LinkyLabelList[i].data);
            break;
        case TYPE_UINT8:
            ESP_LOGI(LINKY_TAG, "%s: %u", LinkyLabelList[i].label, *(uint8_t *)LinkyLabelList[i].data);
            break;
        case TYPE_UINT16:
            ESP_LOGI(LINKY_TAG, "%s: %u", LinkyLabelList[i].label, *(uint16_t *)LinkyLabelList[i].data);
            break;
        case TYPE_UINT32:
            ESP_LOGI(LINKY_TAG, "%s: %lu", LinkyLabelList[i].label, *(uint32_t *)LinkyLabelList[i].data);
            break;
        case TYPE_UINT48:
            ESP_LOGI(LINKY_TAG, "%s: %llu", LinkyLabelList[i].label, *(uint64_t *)LinkyLabelList[i].data);
            break;
        default:
            break;
        }
    }
}

/**
 * @brief Calculate the checksum
 *
 * @param label name of the field
 * @param data value of the field
 * @return return the character of the checksum
 */
char Linky::checksum(char *label, char *data, char *time)
{
    int S1 = 0;                                // sum of the ASCII codes of the characters in the label
    for (int i = 0; i < strlen(label); i++)    // for each character in the label
    {                                          //
        S1 += label[i];                        // add the ASCII code of the label character to the sum
    }                                          //
    S1 += GROUP_SEPARATOR;                     // add the ASCII code of the separator to the sum
    for (int i = 0; i < strlen(data); i++)     // for each character in the data
    {                                          //
        S1 += data[i];                         // add the ASCII code of the data character to the sum
    }                                          //
    if (linky.mode == MODE_STANDARD)           // if the mode is standard
    {                                          //
        S1 += GROUP_SEPARATOR;                 // add the ASCII code of the separator to the sum
        for (int i = 0; i < strlen(time); i++) // for each character in the time
        {                                      //
            S1 += time[i];                     // add the ASCII code of the time character to the sum
        }                                      //
    }                                          //
    return (S1 & 0x3F) + 0x20;                 // return the checksum
}
