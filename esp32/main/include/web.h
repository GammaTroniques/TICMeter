#ifndef WEB_H
#define WEB_H
#include "cJSON.h"
#include "linky.h"
#include "config.h"

void preapareJsonData(LinkyData *data, char dataIndex, char *json, unsigned int jsonSize);

#endif // !WEB_H