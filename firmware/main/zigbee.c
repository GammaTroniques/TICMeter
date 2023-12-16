/**
 * @file zigbee.cpp
 * @author Dorian Benech
 * @brief
 * @version 1.0
 * @date 2023-10-19
 *
 * @copyright Copyright (c) 2023 GammaTroniques
 *
 */

/*==============================================================================
 Local Include
===============================================================================*/
#include "zigbee.h"
#include "esp_zigbee_core.h"
#include "esp_app_desc.h"
#include <stdio.h>
#include <inttypes.h>
#include <inttypes.h>
#include "nvs_flash.h"
#include <string.h>
#include "esp_check.h"

#include "config.h"
/*==============================================================================
 Local Define
===============================================================================*/

#define TAG "ZIGBEE"

/*==============================================================================
 Local Macro
===============================================================================*/
#define DEFINE_PSTRING(var, str)   \
    const struct                   \
    {                              \
        unsigned char len;         \
        char content[sizeof(str)]; \
    }(var) = {sizeof(str) - 1, (str)}

/*==============================================================================
 Local Type
===============================================================================*/
DEFINE_PSTRING(zigbee_device_name, "TICMeter");
DEFINE_PSTRING(zigbee_device_manufacturer, "GammaTroniques");
DEFINE_PSTRING(zigbee_date_code, "2023-12-16");

/*==============================================================================
 Local Function Declaration
===============================================================================*/
static void zigbee_bdb_start_top_level_commissioning_cb(uint8_t mode_mask);
static esp_err_t zigbee_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message);
static esp_err_t zigbee_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message);
static void zigbee_task(void *pvParameters);
static void zigbee_report_attribute(uint8_t endpoint, uint16_t clusterID, uint16_t attributeID, void *value, uint8_t value_length);

/*==============================================================================
Public Variable
===============================================================================*/

/*==============================================================================
 Local Variable
===============================================================================*/

/*==============================================================================
Function Implementation
===============================================================================*/

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
            esp_zb_scheduler_alarm((esp_zb_callback_t)zigbee_bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;
    default:
        ESP_LOGI(TAG, "ZDO signal: %d, status: %d", sig_type, err_status);
        break;
    }
}

static void zigbee_bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_ERROR_CHECK(esp_zb_bdb_start_top_level_commissioning(mode_mask));
}

static esp_err_t zigbee_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
                        message->info.status);
    ESP_LOGI(TAG, "Received message: endpoint(0x%x), cluster(0x%x), attribute(0x%x), data size(%d)", message->info.dst_endpoint, message->info.cluster,
             message->attribute.id, message->attribute.data.size);
    return ret;
}

static esp_err_t zigbee_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    esp_err_t ret = ESP_OK;
    switch (callback_id)
    {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        ret = zigbee_attribute_handler((esp_zb_zcl_set_attr_value_message_t *)message);
        break;
    default:
        ESP_LOGW(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
        break;
    }
    return ret;
}

void zigbee_init_stack()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_LOGI(TAG, "Initializing Zigbee stack");
    esp_zb_platform_config_t config = {};
    config.radio_config.radio_mode = RADIO_MODE_NATIVE;
    config.host_config.host_connection_mode = HOST_CONNECTION_MODE_NONE;

    ESP_ERROR_CHECK(esp_zb_platform_config(&config));
    xTaskCreate(zigbee_task, "Zigbee_main", 8 * 1024, NULL, 5, NULL);
}

static void zigbee_task(void *pvParameters)
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

    const esp_app_desc_t *app_desc = esp_app_get_description();
    uint8_t SWBuildID[35];
    snprintf((char *)SWBuildID, sizeof(SWBuildID), "%c%s", strlen(app_desc->version), app_desc->version);

    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID, &ApplicationVersion);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_STACK_VERSION_ID, &StackVersion);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_HW_VERSION_ID, &HWVersion);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, (void *)&zigbee_device_manufacturer);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, (void *)&zigbee_device_name);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_DATE_CODE_ID, (void *)&zigbee_date_code);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID, SWBuildID);

    // ---------------------- Identify cluster ----------------------
    esp_zb_identify_cluster_cfg_t identify_cluster_cfg = {};
    identify_cluster_cfg.identify_time = 0;
    esp_zb_attribute_list_t *esp_zb_identify_cluster = esp_zb_identify_cluster_create(&identify_cluster_cfg);

    // ---------------------- Metering cluster ----------------------

    esp_zb_metering_cluster_cfg_t metering_cluster_cfg = {
        .uint_of_measure = ESP_ZB_ZCL_METERING_UNIT_KW_KWH_BINARY,
    };
    esp_zb_attribute_list_t *esp_zb_metering_cluster = esp_zb_metering_cluster_create(&metering_cluster_cfg);
    // TODO: wait zigbee update

    //------------------ Electrical Measurement cluster ------------------
    esp_zb_electrical_meas_cluster_cfg_t electrical_meas_cluster_cfg = {
        .measured_type = ESP_ZB_ZCL_ELECTRICAL_MEASUREMENT_APPARENT_MEASUREMENT,
    };

    esp_zb_attribute_list_t *esp_zb_electrical_meas_cluster = esp_zb_electrical_meas_cluster_create(&electrical_meas_cluster_cfg);
    for (int i = 0; i < LinkyLabelListSize; i++)
    {
        if (LinkyLabelList[i].mode != linky_mode && LinkyLabelList[i].mode != ANY)
        {
            continue;
        }
        if (LinkyLabelList[i].data == NULL)
        {
            continue;
        }

        switch (LinkyLabelList[i].type)
        {
        case UINT8:
        {
            if (*(uint8_t *)LinkyLabelList[i].data == UINT8_MAX)
            {
                continue;
            }
            break;
        }
        case UINT16:
        {
            if (*(uint16_t *)LinkyLabelList[i].data == UINT16_MAX)
            {
                continue;
            }
            break;
        }
        case UINT32:
        {
            if (*(uint32_t *)LinkyLabelList[i].data == UINT32_MAX)
            {
                continue;
            }
            break;
        }
        case UINT64:
        {
            if (*(uint64_t *)LinkyLabelList[i].data == UINT64_MAX)
            {
                continue;
            }
            break;
        }
        default:
            break;
        };

        ESP_LOGI(TAG, "Adding %s : Cluster: %x, attribute: %x, value: %d", LinkyLabelList[i].label, LinkyLabelList[i].clusterID, LinkyLabelList[i].attributeID, *(uint16_t *)LinkyLabelList[i].data);

        switch (LinkyLabelList[i].clusterID)
        {
        case ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT:
        {
            esp_zb_electrical_meas_cluster_add_attr(esp_zb_electrical_meas_cluster, LinkyLabelList[i].attributeID, LinkyLabelList[i].data);
            break;
        }
        case ESP_ZB_ZCL_CLUSTER_ID_METERING:
        {
            esp_zb_cluster_add_attr(esp_zb_metering_cluster, ESP_ZB_ZCL_CLUSTER_ID_METERING, LinkyLabelList[i].attributeID, ESP_ZB_ZCL_ATTR_TYPE_48BIT, ESP_ZB_ZCL_ATTR_ACCESS_REPORTING, LinkyLabelList[i].data);
            break;
        }
        default:
            break;
        }
    }

    // esp_zb_electrical_meas_cluster_add_attr(esp_zb_electrical_meas_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMSCURRENT_ID, (uint16_t *)&linky.data.hist.IINST);
    // esp_zb_electrical_meas_cluster_add_attr(esp_zb_electrical_meas_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_MAX_ID, (uint16_t *)&linky.data.hist.IMAX);
    // esp_zb_electrical_meas_cluster_add_attr(esp_zb_electrical_meas_cluster, ESP_ZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_APPARENT_POWER_ID, (uint16_t *)&linky.data.hist.PAPP);

    // ------------------ LinkyCustomCluster ------------------
    char test[] = "test";
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
    esp_zb_cluster_list_add_metering_cluster(esp_zb_cluster_list, esp_zb_metering_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_custom_cluster(esp_zb_cluster_list, esp_zb_linky_custom_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_custom_cluster(esp_zb_cluster_list, esp_zb_meter_custom_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_t temp = *esp_zb_cluster_list;
    temp = temp;

    //------------------ Endpoint list ------------------
    esp_zb_ep_list_t *esp_zb_ep_list = esp_zb_ep_list_create();
    esp_zb_ep_list_add_ep(esp_zb_ep_list, esp_zb_cluster_list, LINKY_TIC_ENDPOINT, ESP_ZB_AF_HA_PROFILE_ID, ESP_ZB_HA_METER_INTERFACE_DEVICE_ID);
    esp_zb_device_register(esp_zb_ep_list);

    //------------------ Callbacks ------------------
    esp_zb_core_action_handler_register(zigbee_action_handler);

    esp_zb_set_primary_network_channel_set(ZIGBEE_CHANNEL_MASK);
    ESP_LOGI(TAG, "Primary channel mask");
    esp_zb_set_secondary_network_channel_set(0x07FFF800); // all channels
    ESP_LOGI(TAG, "Secondary channel mask");
    ESP_ERROR_CHECK(esp_zb_start(false));
    ESP_LOGI(TAG, "Zigbee stack started");
    esp_zb_main_loop_iteration();
}

static void zigbee_report_attribute(uint8_t endpoint, uint16_t clusterID, uint16_t attributeID, void *value, uint8_t value_length)
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

uint8_t zigbee_send(LinkyData *data)
{
    static uint8_t fistCall = 0;
    if (fistCall == 0)
    {
        fistCall = 1;
        zigbee_init_stack();
    }
    for (int i = 0; i < LinkyLabelListSize; i++)
    {
        if (LinkyLabelList[i].mode != linky_mode && LinkyLabelList[i].mode != ANY)
        {
            continue;
        }
        if (LinkyLabelList[i].data == NULL)
        {
            continue;
        }
        if (LinkyLabelList[i].clusterID == 0)
        {
            continue;
        }
        switch (LinkyLabelList[i].type)
        {
        case UINT8:
        {
            if (*(uint8_t *)LinkyLabelList[i].data == UINT8_MAX)
            {
                continue;
            }
            break;
        }
        case UINT16:
        {
            if (*(uint16_t *)LinkyLabelList[i].data == UINT16_MAX)
            {
                continue;
            }
            break;
        }
        case UINT32:
        {
            if (*(uint32_t *)LinkyLabelList[i].data == UINT32_MAX)
            {
                continue;
            }
            break;
        }
        case UINT64:
        {
            if (*(uint64_t *)LinkyLabelList[i].data == UINT64_MAX)
            {
                continue;
            }
            break;
        }
        default:
            break;
        };
        if (LinkyLabelList[i].clusterID == ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT && LinkyLabelList[i].mode == MODE_HISTORIQUE)
        {
            ESP_LOGI(TAG, "Repprting attribute: %s, value: %d", LinkyLabelList[i].label, *(uint16_t *)LinkyLabelList[i].data);
            if (LinkyLabelList[i].realTime == REAL_TIME)
            {
                zigbee_report_attribute(LINKY_TIC_ENDPOINT, LinkyLabelList[i].clusterID, LinkyLabelList[i].attributeID, LinkyLabelList[i].data, LinkyLabelList[i].type);
            }
            else
            {
                esp_zb_zcl_set_attribute_val(LINKY_TIC_ENDPOINT, LinkyLabelList[i].clusterID, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, LinkyLabelList[i].attributeID, LinkyLabelList[i].data, true);
            }
        }
    }
    return 0;
}