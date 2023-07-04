#include "web.h"
#include "config.h"
#include "gpio.h"
#include "wifi.h"

void preapareJsonData(LinkyData *data, char dataIndex, char *json, unsigned int jsonSize)
{
    cJSON *jsonObject = cJSON_CreateObject(); // Create the root object
    cJSON_AddStringToObject(jsonObject, "TOKEN", config.values.web.token);
    cJSON_AddNumberToObject(jsonObject, "VCONDO", getVCondo());
    cJSON *dataObject = cJSON_CreateArray(); // Create the data array
    for (int i = 0; i < dataIndex; i++)      // Add data to the array
    {
        cJSON *dataItem = cJSON_CreateObject();
        cJSON_AddNumberToObject(dataItem, "DATE", data[i].timestamp);
        cJSON_AddStringToObject(dataItem, "ADCO", data[i].ADCO);
        cJSON_AddStringToObject(dataItem, "OPTARIF", data[i].OPTARIF);
        cJSON_AddNumberToObject(dataItem, "ISOUSC", data[i].ISOUSC);
        if (data[i].BASE != 0)
            cJSON_AddNumberToObject(dataItem, "BASE", data[i].BASE);
        if (data[i].HCHC != 0)
            cJSON_AddNumberToObject(dataItem, "HCHC", data[i].HCHC);
        if (data[i].HCHP != 0)
            cJSON_AddNumberToObject(dataItem, "HCHP", data[i].HCHP);
        cJSON_AddStringToObject(dataItem, "PTEC", data[i].PTEC);
        cJSON_AddNumberToObject(dataItem, "IINST", data[i].IINST);
        cJSON_AddNumberToObject(dataItem, "IMAX", data[i].IMAX);
        cJSON_AddNumberToObject(dataItem, "PAPP", data[i].PAPP);
        cJSON_AddStringToObject(dataItem, "HHPHC", data[i].HHPHC);
        cJSON_AddStringToObject(dataItem, "MOTDETAT", data[i].MOTDETAT);
        cJSON_AddItemToArray(dataObject, dataItem);
    }

    if (dataIndex == 0)
    {
        // Send empty data to server to keep the connection alive
        cJSON_AddStringToObject(jsonObject, "ERROR", "Cant read data from linky");
        cJSON *dataItem = cJSON_CreateObject();
        cJSON_AddNumberToObject(dataItem, "DATE", getTimestamp());
        cJSON_AddNullToObject(dataItem, "BASE");
        cJSON_AddNullToObject(dataItem, "HCHC");
        cJSON_AddNullToObject(dataItem, "HCHP");
        cJSON_AddItemToArray(dataObject, dataItem);
    }
    cJSON_AddItemToObject(jsonObject, "data", dataObject); // Add the data array to the root object
    char *jsonString = cJSON_PrintUnformatted(jsonObject); // Convert the json object to string
    strncpy(json, jsonString, jsonSize);                   // Copy the string to the buffer
    free(jsonString);                                      // Free the memory
    cJSON_Delete(jsonObject);                              // Delete the json object
}