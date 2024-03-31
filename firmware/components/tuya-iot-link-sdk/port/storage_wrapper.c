#ifdef __cplusplus
extern "C"
{
#endif

#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "tuya_error_code.h"
#include "storage_interface.h"
#include "system_interface.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

#define STORAGE_NAMESPACE "tuya_storage"
    nvs_handle_t storage_handle;

    int local_storage_init(void)
    {

        esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &storage_handle);
        if (err != ESP_OK)
            return err;

        nvs_stats_t stats;
        ESP_ERROR_CHECK(nvs_get_stats(NULL, &stats));
        printf(
            "Used entries: %3zu\t"
            "Free entries: %3zu\t"
            "Total entries: %3zu\t"
            "Namespace count: %3zu\n",
            stats.used_entries,
            stats.free_entries,
            stats.total_entries,
            stats.namespace_count);

        nvs_iterator_t it = NULL;
        esp_err_t res = nvs_entry_find("nvs", "tuya_storage", NVS_TYPE_ANY, &it);
        ESP_LOGI("TUYA_STORAGE", "nvs_entry_find: 0x%x", res);
        while (res == ESP_OK)
        {
            nvs_entry_info_t info;
            nvs_entry_info(it, &info); // Can omit error check if parameters are guaranteed to be non-NULL
            size_t size = 0;
            nvs_get_blob(storage_handle, info.key, NULL, &size);
            printf("Namespace: %s\tKey: %s\tType: %d\tSize: %d\n", info.namespace_name, info.key, info.type, size);
            res = nvs_entry_next(&it);
        }
        nvs_release_iterator(it);
        return OPRT_OK;
    }

    int local_storage_clear(void)
    {
        ESP_LOGE("TUYA_STORAGE", "local_storage_clear");
        esp_err_t err = nvs_flash_erase();
        if (err != ESP_OK)
        {
            ESP_LOGE("TUYA_STORAGE", "nvs_flash_erase failed: 0x%x", err);
            return err;
        }
        return OPRT_OK;
    }
    int local_storage_set(const char *key, const uint8_t *buffer, size_t length)
    {
        if (NULL == key || NULL == buffer)
        {
            return OPRT_INVALID_PARM;
        }

        log_debug("set key:%s", key);

        esp_err_t err;

        err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &storage_handle);
        if (err != ESP_OK)
            return err;

        err = nvs_set_blob(storage_handle, key, buffer, length);
        // ESP_LOGE("TUYA_STORAGE", "local_storage_set %s %d bytes", key, length);
        if (err != ESP_OK)
            return err;

        // Commit
        err = nvs_commit(storage_handle);
        if (err != ESP_OK)
            return err;

        // Close
        nvs_close(storage_handle);

        return OPRT_OK;
    }

    int local_storage_get(const char *key, uint8_t *buffer, size_t *length)
    {
        if (NULL == key || NULL == buffer || NULL == length)
        {
            return OPRT_INVALID_PARM;
        }

        log_debug("get key:%s, len:%d", key, (int)*length);

        esp_err_t err;

        err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &storage_handle);
        if (err != ESP_OK)
            return err;

        err = nvs_get_blob(storage_handle, key, NULL, length);
        // ESP_LOGE("TUYA_STORAGE", "local_storage_get %s %d bytes", key, *length);
        if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            return err;
        }
        log_debug("get key:%s, xlen:%d", key, *length);

        err = nvs_get_blob(storage_handle, key, buffer, length);
        // ESP_LOGE("TUYA_STORAGE", "local_storage_get %s %d bytes", key, *length);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
            return err;

        // Close
        nvs_close(storage_handle);

        return OPRT_OK;
    }

    int local_storage_del(const char *key)
    {
        log_debug("key:%s", key);

        esp_err_t err;

        err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &storage_handle);
        if (err != ESP_OK)
            return err;

        err = nvs_erase_key(storage_handle, key);
        ESP_LOGE("TUYA_STORAGE", "local_storage_del: %s", key);
        if (err != ESP_OK)
            return err;

        // Close
        nvs_close(storage_handle);

        return OPRT_OK;
    }

#ifdef __cplusplus
}
#endif
