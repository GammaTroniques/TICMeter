/**
 * @file web.h
 * @author Dorian Benech
 * @brief
 * @version 1.0
 * @date 2023-10-11
 *
 * @copyright Copyright (c) 2023 GammaTroniques
 *
 */

#ifndef WEB_H
#define WEB_H

/*==============================================================================
 Local Include
===============================================================================*/
#include "cJSON.h"
#include "linky.h"
#include "config.h"

/*==============================================================================
 Public Defines
==============================================================================*/

/*==============================================================================
 Public Macro
==============================================================================*/

/*==============================================================================
 Public Type
==============================================================================*/

/*==============================================================================
 Public Variables Declaration
==============================================================================*/

/*==============================================================================
 Public Functions Declaration
==============================================================================*/

/**
 * @brief prepare json data to send to server
 *
 * @param data  the Array of data to send
 * @param dataIndex  the index of the data to send
 * @param json  the json destination
 * @param jsonSize the size of the json destination
 */
extern void web_preapare_json_data(LinkyData *data, char dataIndex, char *json, unsigned int jsonSize);

#endif /* WEB_H */