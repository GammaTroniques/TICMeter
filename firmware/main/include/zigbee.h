#ifndef ZIGBEE_H
#define ZIGBEE_H

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ha/esp_zigbee_ha_standard.h"
#include "ha/zb_ha_device_config.h"
#include "zcl/esp_zigbee_zcl_power_config.h"
#include "linky.h"

#define LINKY_TIC_ENDPOINT 1
#define ZIGBEE_CHANNEL_MASK (1l << 15)
void init_zigbee();
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct);
static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask);
void attr_cb(uint8_t status, uint8_t endpoint, uint16_t cluster_id, uint16_t attr_id, void *new_value);

void zigbee_task(void *pvParameters);
uint8_t sendToZigbee(LinkyData *data);

#endif // ZIGBEE_H