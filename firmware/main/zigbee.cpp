#include "zigbee.h"
#include "esp_zigbee_core.h"
#include "esp_app_desc.h"
#include <stdio.h>
#include <inttypes.h>
#include <inttypes.h>
#include "nvs_flash.h"
#include <string.h>

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
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_LOGI(TAG, "Initializing Zigbee stack");
    esp_zb_platform_config_t config = {};
    config.radio_config.radio_mode = RADIO_MODE_NATIVE;
    config.host_config.host_connection_mode = HOST_CONNECTION_MODE_NONE;

    ESP_ERROR_CHECK(esp_zb_platform_config(&config));
    xTaskCreate(zigbee_task, "Zigbee_main", 4096, NULL, 5, NULL);
}

void zigbee_task(void *pvParameters)
{
    // clang-format on
    // ESP_LOGI(TAG, "Resetting Zigbee stack");
    // esp_zb_factory_reset();
    // ESP_LOGI(TAG, "Starting Zigbee stack");
    /* initialize Zigbee stack with Zigbee end-device config */
    esp_zb_cfg_t zigbee_cfg = {};
    zigbee_cfg.esp_zb_role = ESP_ZB_DEVICE_TYPE_ED;
    zigbee_cfg.install_code_policy = false;
    zigbee_cfg.nwk_cfg.zed_cfg.ed_timeout = ESP_ZB_ED_AGING_TIMEOUT_64MIN;
    zigbee_cfg.nwk_cfg.zed_cfg.keep_alive = 3000; // in seconds
    esp_zb_init(&zigbee_cfg);
    ESP_LOGI(TAG, "Zigbee stack initialized");
    //------------------ Basic cluster ------------------
    esp_zb_basic_cluster_cfg_t basic_cluster_cfg = {};
    basic_cluster_cfg.zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE;
    esp_zb_attribute_list_t *esp_zb_basic_cluster = esp_zb_basic_cluster_create(&basic_cluster_cfg);

    uint8_t ApplicationVersion = 1;
    uint8_t StackVersion = 1;
    uint8_t HWVersion = 1;
    uint8_t DateCode[] = {8, '2', '0', '2', '3', '0', '9', '1', '0'};
    uint8_t ManufacturerName[] = {14, 'G', 'a', 'm', 'm', 'a', 'T', 'r', 'o', 'n', 'i', 'q', 'u', 'e', 's'};
    uint8_t ModelIdentifier[] = {9, 'T', 'I', 'C', ' ', 'M', 'e', 't', 'e', 'r'};

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

    // ---------------------- Identify cluster ----------------------
    esp_zb_identify_cluster_cfg_t identify_cluster_cfg = {};
    identify_cluster_cfg.identify_time = 0;
    esp_zb_attribute_list_t *esp_zb_identify_cluster = esp_zb_identify_cluster_create(&identify_cluster_cfg);

    // ---------------------- Metering cluster ----------------------
    // TODO: wait zigbee update

    //------------------ Electrical Measurement cluster ------------------
    esp_zb_electrical_meas_cluster_cfg_t electrical_meas_cluster_cfg = {};
    electrical_meas_cluster_cfg.measured_type = ESP_ZB_ZCL_ELECTRICAL_MEASUREMENT_APPARENT_MEASUREMENT;

    esp_zb_attribute_list_t *esp_zb_electrical_meas_cluster = esp_zb_electrical_meas_cluster_create(&electrical_meas_cluster_cfg);
    for (int i = 0; i < LinkyLabelListSize; i++)
    {
        if (LinkyLabelList[i].clusterID == ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT && LinkyLabelList[i].mode == MODE_HISTORIQUE)
        {
            ESP_LOGI(TAG, "Adding attribute: %s, value: %d", LinkyLabelList[i].label, *(uint16_t *)LinkyLabelList[i].data);
            esp_zb_electrical_meas_cluster_add_attr(esp_zb_electrical_meas_cluster, LinkyLabelList[i].attributeID, LinkyLabelList[i].data);
        }
    }

    // esp_zb_electrical_meas_cluster_add_attr(esp_zb_electrical_meas_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSCURRENT_ID, (uint16_t *)&linky.data.hist.IINST);
    // esp_zb_electrical_meas_cluster_add_attr(esp_zb_electrical_meas_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MAX_ID, (uint16_t *)&linky.data.hist.IMAX);
    // esp_zb_electrical_meas_cluster_add_attr(esp_zb_electrical_meas_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_APPARENT_POWER_ID, (uint16_t *)&linky.data.hist.PAPP);

    // ------------------ LinkyCustomCluster ------------------
    char *test = "test";
    esp_zb_attribute_list_t *esp_zb_linky_custom_cluster = esp_zb_zcl_attr_list_create(0xFF66);
    esp_zb_custom_cluster_add_custom_attr(esp_zb_linky_custom_cluster, 0x0001, ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING, ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, test);
    esp_zb_custom_cluster_add_custom_attr(esp_zb_linky_custom_cluster, 0x0002, ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING, ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, test);

    //------------------ Metering cluster ------------------
    char test0 = 0;
    esp_zb_attribute_list_t *esp_zb_meter_custom_cluster = esp_zb_zcl_attr_list_create(0xFF00);
    esp_zb_custom_cluster_add_custom_attr(esp_zb_linky_custom_cluster, 0x0000, ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING, ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, &test0);
    esp_zb_custom_cluster_add_custom_attr(esp_zb_linky_custom_cluster, 0x0308, ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY, test);
    esp_zb_meter_custom_cluster->attribute.id = 0x0702;

    //------------------ Cluster list ------------------
    esp_zb_cluster_list_t *esp_zb_cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(esp_zb_cluster_list, esp_zb_identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_electrical_meas_cluster(esp_zb_cluster_list, esp_zb_electrical_meas_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_custom_cluster(esp_zb_cluster_list, esp_zb_linky_custom_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_custom_cluster(esp_zb_cluster_list, esp_zb_meter_custom_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_t temp = *esp_zb_cluster_list;
    temp = temp;

    //------------------ Endpoint list ------------------
    esp_zb_ep_list_t *esp_zb_ep_list = esp_zb_ep_list_create();
    esp_zb_ep_list_add_ep(esp_zb_ep_list, esp_zb_cluster_list, LINKY_TIC_ENDPOINT, ESP_ZB_AF_HA_PROFILE_ID, ESP_ZB_HA_METER_INTERFACE_DEVICE_ID);
    esp_zb_device_register(esp_zb_ep_list);

    //------------------ Callbacks ------------------
    esp_zb_device_add_set_attr_value_cb(attr_cb);
    // esp_zb_device_add_report_attr_cb(attr_cb);

    esp_zb_set_primary_network_channel_set(ZIGBEE_CHANNEL_MASK);
    ESP_LOGI(TAG, "Primary channel mask");
    esp_zb_set_secondary_network_channel_set(0x07FFF800); // all channels
    ESP_LOGI(TAG, "Secondary channel mask");
    ESP_ERROR_CHECK(esp_zb_start(false));
    ESP_LOGI(TAG, "Zigbee stack started");
    esp_zb_main_loop_iteration();
}

void reportAttribute(uint8_t endpoint, uint16_t clusterID, uint16_t attributeID, void *value, uint8_t value_length)
{
    esp_zb_zcl_report_attr_cmd_t cmd = {
        .zcl_basic_cmd = {
            .dst_addr_u = {
                .addr_short = 0x0000,
            },
            .dst_endpoint = endpoint,
            .src_endpoint = endpoint,
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .clusterID = clusterID,
        .cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        .attributeID = attributeID,
    };
    esp_zb_zcl_attr_t *value_r = esp_zb_zcl_get_attribute(endpoint, clusterID, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, attributeID);
    memcpy(value_r->data_p, value, value_length);
    esp_zb_zcl_report_attr_cmd_req(&cmd);
}

uint8_t sendToZigbee(LinkyData *data)
{
    static uint8_t fistCall = 0;
    if (fistCall == 0)
    {
        fistCall = 1;
        init_zigbee();
    }
    for (int i = 0; i < LinkyLabelListSize; i++)
    {
        if (LinkyLabelList[i].clusterID == ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT && LinkyLabelList[i].mode == MODE_HISTORIQUE)
        {
            ESP_LOGI(TAG, "Repprting attribute: %s, value: %d", LinkyLabelList[i].label, *(uint16_t *)LinkyLabelList[i].data);
            if (LinkyLabelList[i].realTime == REAL_TIME)
            {
                reportAttribute(LINKY_TIC_ENDPOINT, LinkyLabelList[i].clusterID, LinkyLabelList[i].attributeID, LinkyLabelList[i].data, LinkyLabelList[i].type);
            }
            else
            {
                esp_zb_zcl_set_attribute_val(LINKY_TIC_ENDPOINT, LinkyLabelList[i].clusterID, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, LinkyLabelList[i].attributeID, LinkyLabelList[i].data, true);
            }
        }
    }
    return 0;
}