/**
 * @file ota_zlib.c
 * @author Dorian Benech
 * @brief
 * @version 1.0
 * @date 2023-10-11
 *
 * @copyright Copyright (c) 2023 GammaTroniques
 *
 */

/*==============================================================================
 Local Include
===============================================================================*/
#include "ota_zlib.h"
#include "zlib.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_partition.h"
#include "esp_err.h"
#include <stdio.h>
#include <string.h>
#include "esp_check.h"

/*==============================================================================
 Local Define
===============================================================================*/
#define TAG "OTA_ZLIB"
/*==============================================================================
 Local Macro
===============================================================================*/

/*==============================================================================
 Local Type
===============================================================================*/

/*==============================================================================
 Local Function Declaration
===============================================================================*/

/*==============================================================================
Public Variable
===============================================================================*/

/*==============================================================================
 Local Variable
===============================================================================*/
z_stream zlib_stream = {0};
uint8_t zlib_init = 0;
uint8_t zlib_buf[2048];

static const esp_partition_t *zigbee_ota_partition = NULL;
static const esp_partition_t *zigbee_storage_partition = NULL;
static esp_ota_handle_t zigbee_ota_handle = 0;

static size_t ota_data_len_ = 0;
static size_t ota_header_len_ = 0;
static bool ota_upgrade_subelement_ = false;
static uint8_t ota_header_[6] = {0};
static uint16_t subelement_type = 0;
static uint32_t subelement_size = 0;
static uint32_t storage_offset = 0;
static uint32_t remaining_data = 0;

uint8_t zigbee_ota_between_packet[2048];
uint16_t zigbee_ota_between_packet_size = 0;

/*==============================================================================
Function Implementation
===============================================================================*/

esp_err_t ota_zlib_init()
{
    ota_upgrade_subelement_ = false;
    ota_data_len_ = 0;
    ota_header_len_ = 0;

    int ret = inflateInit(&zlib_stream);

    if (ret == Z_OK)
    {
        zlib_init = 1;
    }
    else
    {
        ESP_LOGE(TAG, "zlib init failed: %d", ret);
        return false;
    }

    if (zigbee_ota_partition != NULL)
    {
        ESP_LOGE(TAG, "OTA already started");
        zigbee_ota_partition = NULL;
        esp_ota_abort(zigbee_ota_handle);
        return ESP_FAIL;
    }

    zigbee_ota_partition = esp_ota_get_next_update_partition(NULL);
    if (zigbee_ota_partition == NULL)
    {
        ESP_LOGE(TAG, "OTA partition not found");
        return ESP_FAIL;
    }
    zigbee_storage_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "storage");
    if (zigbee_storage_partition == NULL)
    {
        ESP_LOGE(TAG, "Storage partition not found");
        return ESP_FAIL;
    }

    ret = esp_ota_begin(zigbee_ota_partition, OTA_WITH_SEQUENTIAL_WRITES, &zigbee_ota_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to begin OTA partition, status: %s", esp_err_to_name(ret));
        zigbee_ota_partition = NULL;
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Init OK");
    return ESP_OK;
}

esp_err_t ota_zlib_write(const uint8_t *payload, size_t payload_size)
{
    esp_err_t ret = ESP_OK;
    static uint32_t offset = 0;
    offset += payload_size;

    if (zigbee_ota_between_packet_size > 0)
    {
        ESP_LOGI(TAG, "OTA between packet: %d", zigbee_ota_between_packet_size);
        memcpy(zigbee_ota_between_packet + zigbee_ota_between_packet_size, payload, payload_size);
        payload = zigbee_ota_between_packet;
        payload_size += zigbee_ota_between_packet_size;
        // ESP_LOG_BUFFER_HEXDUMP(TAG, payload, payload_size, ESP_LOG_INFO);
        zigbee_ota_between_packet_size = 0;
    }

    while (ota_header_len_ < 6 && payload_size > 0)
    {
        if ((payload[0] | payload[1] << 8 | payload[2] << 16 | payload[3] << 24) == 0x0BEEF11E)
        {
            ESP_LOGI(TAG, "We have OTA Zigbee header, skip it");
            payload += 56;
            payload_size -= 56;
            continue;
        }

        ota_header_[ota_header_len_] = payload[0];
        ota_header_len_++;
        payload++;
        payload_size--;
        ESP_LOGI(TAG, "OTA header len %zu:", ota_header_len_);
        // ESP_LOG_BUFFER_HEXDUMP(TAG, ota_header_, ota_header_len_, ESP_LOG_WARN);
        // ESP_LOG_BUFFER_HEXDUMP(TAG, payload, payload_size, ESP_LOG_WARN);
    }

    if (!ota_upgrade_subelement_ && ota_header_len_ == 6)
    {
        subelement_type = (ota_header_[1] << 8) | ota_header_[0];
        subelement_size = (((int)ota_header_[5] & 0xFF) << 24) | (((int)ota_header_[4] & 0xFF) << 16) | (((int)ota_header_[3] & 0xFF) << 8) | ((int)ota_header_[2] & 0xFF);
        ota_upgrade_subelement_ = true;
        switch (subelement_type)
        {
        case 0:
            ota_data_len_ = subelement_size;
            ESP_LOGW(TAG, "OTA sub-element app [%lu/%lu]", offset, subelement_size);
            break;

        case 0x100:
            ESP_LOGI(TAG, "OTA sub-element storage [%lu/%lu]", offset, subelement_size);
            ota_data_len_ = subelement_size;
            storage_offset = 0;
            break;
        default:
            ESP_LOGE(TAG, "OTA sub-element type %02x%02x not supported", ota_header_[0], ota_header_[1]);
            return ESP_FAIL;

            break;
        }
    }

    if (ota_data_len_)
    {
        if (zlib_init == 0)
        {
            int ret = inflateInit(&zlib_stream);
            if (ret == Z_OK)
            {
                zlib_init = 1;
            }
            else
            {
                ESP_LOGE(TAG, "zlib init failed: %d", ret);
                return ESP_FAIL;
            }
        }

        remaining_data = payload_size - ota_data_len_;
        payload_size = MIN(ota_data_len_, payload_size);
        ota_data_len_ -= payload_size;

        zlib_stream.avail_in = payload_size;
        zlib_stream.next_in = (Bytef *)payload;

        do
        {
            zlib_stream.avail_out = sizeof(zlib_buf);
            zlib_stream.next_out = zlib_buf;

            ret = inflate(&zlib_stream, Z_NO_FLUSH);
            if (ret == Z_STREAM_ERROR || ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR)
            {
                ESP_LOGE(TAG, "zlib inflate failed: %d", ret);
                return ESP_FAIL;
            }

            size_t have = sizeof(zlib_buf) - zlib_stream.avail_out;
            if (have)
            {
                switch (subelement_type)
                {
                case 0:
                    ESP_LOGI(TAG, "OTA sub-element Firmware [%lu/%lu]", offset, subelement_size);
                    ret = esp_ota_write(zigbee_ota_handle, zlib_buf, have);
                    if (ret != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Failed to write OTA data to partition, status: %s", esp_err_to_name(ret));
                        return ESP_FAIL;
                    }
                    break;
                case 0x100:
                    ESP_LOGI(TAG, "OTA sub-element Storage [%lu/%lu]", subelement_size - ota_data_len_, subelement_size);
                    ret = esp_partition_write(zigbee_storage_partition, storage_offset, zlib_buf, have);
                    storage_offset += have;
                    if (ret != ESP_OK)
                    {
                        ESP_LOGE(TAG, "Failed to write OTA data to storage partition, status: 0x%x %s", ret, esp_err_to_name(ret));
                        return ESP_FAIL;
                    }
                    break;
                default:
                    break;
                }
            }
        } while (zlib_stream.avail_out == 0);
    }

    if (ota_data_len_ == 0)
    {
        inflateEnd(&zlib_stream);
        zlib_init = 0;
        // ESP_LOGI(TAG, "OTA data len 0, remaining data %lu", remaining_data);
        payload += payload_size;
        // ESP_LOG_BUFFER_HEXDUMP(TAG, payload, remaining_data, ESP_LOG_WARN);

        ota_upgrade_subelement_ = false;
        ota_header_len_ = 0;
        while (ota_header_len_ < 6 && payload_size > 0)
        {
            ota_header_[ota_header_len_] = payload[0];
            ota_header_len_++;
            payload_size--;
            remaining_data--;
            payload++;
            // ESP_LOGI(TAG, "OTA header len END %zu:", ota_header_len_);
            // ESP_LOG_BUFFER_HEXDUMP(TAG, ota_header_, ota_header_len_, ESP_LOG_WARN);
            // ESP_LOG_BUFFER_HEXDUMP(TAG, messsage.payload, messsage.payload_size, ESP_LOG_WARN);
        }
        if (remaining_data)
        {
            ESP_LOGI(TAG, "Save remaining data %lu", remaining_data);
            // ESP_LOG_BUFFER_HEXDUMP(TAG, payload, remaining_data, ESP_LOG_WARN);
            memcpy(zigbee_ota_between_packet, payload, remaining_data);
            zigbee_ota_between_packet_size = remaining_data;
        }
    }
    return ESP_OK;
}

static void reboot_task(void *pvParameter)
{
    vTaskDelay(10000 / portTICK_PERIOD_MS);
    esp_restart();
}

esp_err_t ota_zlib_end()
{
    esp_err_t ret = ESP_OK;
    ret = esp_ota_end(zigbee_ota_handle);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to end OTA partition, status: %s", esp_err_to_name(ret));
    ret = esp_ota_set_boot_partition(zigbee_ota_partition);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to set OTA boot partition, status: %s", esp_err_to_name(ret));
    ESP_LOGW(TAG, "Prepare to restart system in 10s");
    xTaskCreate(&reboot_task, "reboot_task", 2048, NULL, 20, NULL);
    return ESP_OK;
}