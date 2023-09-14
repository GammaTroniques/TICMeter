#ifdef __cplusplus
extern "C" {
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

#define STORAGE_NAMESPACE "tuya_storage"
nvs_handle_t storage_handle;

int local_storage_set(const char* key, const uint8_t* buffer, size_t length)
{
    if (NULL == key || NULL == buffer) {
        return OPRT_INVALID_PARM;
    }

    log_debug("set key:%s", key);

    esp_err_t err;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &storage_handle);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(storage_handle, key, buffer, length);
    if (err != ESP_OK) return err;

    // Commit
    err = nvs_commit(storage_handle);
    if (err != ESP_OK) return err;

    // Close
    nvs_close(storage_handle);

    return OPRT_OK;
}

int local_storage_get(const char* key, uint8_t* buffer, size_t* length)
{
    if (NULL == key || NULL == buffer || NULL == length) {
        return OPRT_INVALID_PARM;
    }

    log_debug("get key:%s, len:%d", key, (int)*length);

    esp_err_t err;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &storage_handle);
    if (err != ESP_OK) return err;

    err = nvs_get_blob(storage_handle, key, NULL, length);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return err;
    }
    log_debug("get key:%s, xlen:%d", key, *length);

    err = nvs_get_blob(storage_handle, key, buffer, length);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;

    // Close
    nvs_close(storage_handle);

    return OPRT_OK;
}

int local_storage_del(const char* key)
{
    log_debug("key:%s", key);

    esp_err_t err;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &storage_handle);
    if (err != ESP_OK) return err;

    err = nvs_erase_key(storage_handle, key);
    if (err != ESP_OK) return err;

    // Close
    nvs_close(storage_handle);

    return OPRT_OK;
}

#ifdef __cplusplus
}
#endif
