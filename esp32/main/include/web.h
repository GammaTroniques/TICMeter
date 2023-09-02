#ifndef WEB_H
#define WEB_H
#include "cJSON.h"
#include "linky.h"
#include "config.h"

/**
 * @brief prepare json data to send to server
 *
 * @param data  the Array of data to send
 * @param dataIndex  the index of the data to send
 * @param json  the json destination
 * @param jsonSize the size of the json destination
 */
void preapareJsonData(LinkyData *data, char dataIndex, char *json, unsigned int jsonSize);

#endif // !WEB_H