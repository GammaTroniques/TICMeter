#include "zigbee.h"
#include "esp_zigbee_core.h"
#include "esp_app_desc.h"
#include <stdio.h>
#include <inttypes.h>
#include <inttypes.h>

#include "config.h"

#define TAG "ZIGBEE"

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    esp_zb_app_signal_type_t *p_sg_p = (esp_zb_app_signal_type_t *)signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;

    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    switch (sig_type)
    {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Zigbee stack initialized");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK)
        {
            ESP_LOGI(TAG, "Start network steering");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        }
        else
        {
            /* commissioning failed */
            ESP_LOGW(TAG, "Failed to initialize Zigbee stack (status: %d)", err_status);
        }
        break;

    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK)
        {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel());
        }
        else
        {
            ESP_LOGI(TAG, "Network steering was not successful (status: %d)", err_status);
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;
    default:
        ESP_LOGI(TAG, "ZDO signal: %d, status: %d", sig_type, err_status);
        break;
    }
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_ERROR_CHECK(esp_zb_bdb_start_top_level_commissioning(mode_mask));
}

void attr_cb(uint8_t status, uint8_t endpoint, uint16_t cluster_id, uint16_t attr_id, void *new_value)
{
    ESP_LOGI(TAG, "cluster:0x%x, attribute:0x%x changed ", cluster_id, attr_id);
}

void init_zigbee()
{
    ESP_LOGI(TAG, "Initializing Zigbee stack");
    esp_zb_platform_config_t config = {};
    config.radio_config.radio_mode = RADIO_MODE_NATIVE;
    config.host_config.host_connection_mode = HOST_CONNECTION_MODE_NONE;

    ESP_ERROR_CHECK(esp_zb_platform_config(&config));
    xTaskCreate(zigbee_task, "Zigbee_main", 4096, NULL, 5, NULL);
}

static void zigbee_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting Zigbee stack");
    /* initialize Zigbee stack with Zigbee end-device config */
    esp_zb_cfg_t zigbee_cfg = {};
    zigbee_cfg.esp_zb_role = ESP_ZB_DEVICE_TYPE_ED;
    zigbee_cfg.install_code_policy = false;
    zigbee_cfg.nwk_cfg.zed_cfg.ed_timeout = ESP_ZB_ED_AGING_TIMEOUT_64MIN;
    zigbee_cfg.nwk_cfg.zed_cfg.keep_alive = 3000; // in seconds
    esp_zb_init(&zigbee_cfg);

    //------------------ Basic cluster ------------------
    esp_zb_basic_cluster_cfg_t basic_cluster_cfg = {};
    basic_cluster_cfg.zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE;
    esp_zb_attribute_list_t *esp_zb_basic_cluster = esp_zb_basic_cluster_create(&basic_cluster_cfg);

    uint8_t ApplicationVersion = 1;
    uint8_t StackVersion = 1;
    uint8_t HWVersion = 1;
    uint8_t ManufacturerName[] = {14, 'G', 'a', 'm', 'm', 'a', 'T', 'r', 'o', 'n', 'i', 'q', 'u', 'e', 's'};
    uint8_t ModelIdentifier[] = {9, 'L', 'i', 'n', 'k', 'y', ' ', 'T', 'I', 'C'};
    uint8_t DateCode[] = {8, '2', '0', '2', '3', '0', '7', '0', '1'};

    const esp_app_desc_t *app_desc = esp_app_get_description();
    uint8_t SWBuildID[35];
    sprintf((char *)SWBuildID, "%c%s", strlen(app_desc->version), app_desc->version);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID, &ApplicationVersion);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_STACK_VERSION_ID, &StackVersion);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_HW_VERSION_ID, &HWVersion);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, ManufacturerName);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, ModelIdentifier);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_DATE_CODE_ID, DateCode);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID, SWBuildID);

    //------------------ Cluster list ------------------
    esp_zb_cluster_list_t *esp_zb_cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    //------------------ Endpoint list ------------------
    esp_zb_ep_list_t *esp_zb_ep_list = esp_zb_ep_list_create();
    esp_zb_ep_list_add_ep(esp_zb_ep_list, esp_zb_cluster_list, LINKY_TIC_ENDPOINT, ESP_ZB_AF_HA_PROFILE_ID, ESP_ZB_HA_ON_OFF_OUTPUT_DEVICE_ID);
    esp_zb_device_register(esp_zb_ep_list);

    //------------------ Callbacks ------------------
    esp_zb_device_add_set_attr_value_cb(attr_cb);
    // esp_zb_device_add_report_attr_cb(attr_cb);

    esp_zb_set_primary_network_channel_set(ZIGBEE_CHANNEL_MASK);
    esp_zb_set_secondary_network_channel_set(0x07FFF800); // all channels
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_main_loop_iteration();
}
