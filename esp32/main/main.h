#include "linky.h"

/**
 * @brief Get the tension of the condo
 *
 * @return float
 */
float getVCondo();

/**
 * @brief Get the Timestamp in seconds
 *
 * @return timestamp
 */
unsigned long getTimestamp();

/**
 * @brief Create a Http Url (http://host/path)
 *
 * @param url the destination url
 * @param host the host
 * @param path the path
 */
void createHttpUrl(char *url, const char *host, const char *path);

/**
 * @brief Get config from server and save it in EEPROM
 *
 */
void getConfigFromServer();

/**
 * @brief prepare json data to send to server
 *
 * @param data  the Array of data to send
 * @param dataIndex  the index of the data to send
 * @param json  the json destination
 * @param jsonSize the size of the json destination
 */
void preapareJsonData(LinkyData *data, char dataIndex, char *json, unsigned int jsonSize);

/**
 * @brief Send json data to server
 *
 * @param json the json data to send
 * @return the http code
 */
char sendToServer(char *json);

/**
 * @brief set the CPU frequency to 240Mhz and connect to wifi
 *
 * @return 1 if connected, 0 if not
 */
char connectToWifi();

/**
 * @brief set the CPU frequency to 10Mhz and disconnect from wifi
 *
 */
void disconectFromWifi();

void fetchLinkyDataTask(void *pvParameters);

void pushButtonTask(void *pvParameters);