/*
 * SPDX-FileCopyrightText: 2017-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"
#include "esp_efuse.h"
#include <assert.h>
#include "efuse_table.h"

// md5_digest_table acf803a762a87d24a2a934519a25fbc0
// This file was generated from the file efuse_table.csv. DO NOT CHANGE THIS FILE MANUALLY.
// If you want to change some fields, you need to change efuse_table.csv file
// then run `efuse_common_table` or `efuse_custom_table` command it will generate this file.
// To show efuse_table run the command 'show_efuse_table'.

static const esp_efuse_desc_t USER_DATA_SERIALNUMBER[] = {
    {EFUSE_BLK3, 0, 96}, // Serial number (12 bytes * 8) or 27 bits for SN,
};

const esp_efuse_desc_t *ESP_EFUSE_USER_DATA_SERIALNUMBER[] = {
    &USER_DATA_SERIALNUMBER[0], // Serial number (12 bytes * 8) or 27 bits for SN
    NULL};
