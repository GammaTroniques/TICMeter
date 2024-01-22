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
#include "gpio.h"
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

typedef enum
{
    ZIGBEE_NOT_CONNECTED,
    ZIGBEE_CONNECTING,
    ZIGBEE_CONNECTED,
} zigbee_state_t;

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
extern const char *BUILD_TIME;

/*==============================================================================
 Local Variable
===============================================================================*/

DEFINE_PSTRING(zigbee_device_name, "TICMeter");
DEFINE_PSTRING(zigbee_device_manufacturer, "GammaTroniques");
// DEFINE_PSTRING(zigbee_date_code, BUILD_TIME);

zigbee_state_t zigbee_state = ZIGBEE_NOT_CONNECTED;

/*==============================================================================
Function Implementation
===============================================================================*/
void zigbee_start_pairing()
{
    esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
}

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
        // ESP_LOGW(TAG, "Signal: %x", sig_type);
        zigbee_state = ZIGBEE_CONNECTING;
        if (err_status == ESP_OK)
        {
            switch (config_values.zigbee.state)
            {
            case ZIGBEE_WANT_PAIRING:
                xTaskCreate(gpio_led_task_pairing, "gpio_led_task_pairing", 2048, NULL, PRIORITY_LED_PAIRING, &gpio_led_pairing_task_handle);
                // fall through
            case ZIGBEE_PAIRING:
                ESP_LOGI(TAG, "Start network steering");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
                break;
            default:
                break;
            }
        }
        else
        {
            /* commissioning failed */
            ESP_LOGW(TAG, "Commissioning failed (status: %d)", err_status);
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
            if (config_values.zigbee.state != ZIGBEE_PAIRED)
            {
                ESP_LOGI(TAG, "Zigbee paired");
                config_values.zigbee.state = ZIGBEE_PAIRED;
                config_write();
            }
            zigbee_state = ZIGBEE_CONNECTED;
        }
        else
        {
            ESP_LOGI(TAG, "Network steering was not successful (status: %d)", err_status);
            esp_zb_scheduler_alarm((esp_zb_callback_t)zigbee_bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
            zigbee_state = ZIGBEE_CONNECTING;
        }
        break;
    case ESP_ZB_COMMON_SIGNAL_CAN_SLEEP:
        // ESP_LOGW(TAG, "Can sleep");
        esp_zb_sleep_now();
        break;
    case ESP_ZB_ZDO_SIGNAL_LEAVE:
        ESP_LOGI(TAG, "Leave signal");
        config_values.zigbee.state = ZIGBEE_NOT_CONFIGURED;
        config_write();
        break;
    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type, esp_err_to_name(err_status));

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
        ESP_LOGW(TAG, "Receive Zigbee action(0x%x) callback, message: %s", callback_id, (char *)message);
        break;
    }
    return ret;
}

void zigbee_init_stack()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_LOGI(TAG, "Initializing Zigbee stack");
    esp_zb_platform_config_t config = {
        .radio_config.radio_mode = RADIO_MODE_NATIVE,
        .host_config.host_connection_mode = HOST_CONNECTION_MODE_NONE,
    };

    ESP_ERROR_CHECK(esp_zb_platform_config(&config));
    xTaskCreate(zigbee_task, "Zigbee_main", 8 * 1024, NULL, PRIORITY_ZIGBEE, NULL);
}

static void zigbee_task(void *pvParameters)
{
    // clang-format on
    // ESP_LOGI(TAG, "Resetting Zigbee stack");
    // esp_zb_factory_reset();
    // ESP_LOGI(TAG, "Starting Zigbee stack");
    /* initialize Zigbee stack with Zigbee end-device config */
    // esp_zb_set_trace_level_mask(ESP_ZB_TRACE_LEVEL_WARN, ESP_ZB_TRACE_SUBSYSTEM_NWK | ESP_ZB_TRACE_SUBSYSTEM_APP);
    // esp_zb_set_trace_level_mask(ESP_ZB_TRACE_LEVEL_INFO, ESP_ZB_TRACE_SUBSYSTEM_COMMON);
    esp_zb_cfg_t zigbee_cfg = {
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,
        .install_code_policy = false,
        .nwk_cfg.zed_cfg.ed_timeout = ESP_ZB_ED_AGING_TIMEOUT_2MIN,
        // .nwk_cfg.zed_cfg.keep_alive = 3000, // in seconds
    };

    ESP_LOGW(TAG, "Enable sleep");
    esp_zb_sleep_enable(true);
    // esp_zb_sleep_set_threshold(1000);
    esp_zb_init(&zigbee_cfg);
    // esp_zb_sleep_set_threshold(500);

    ESP_LOGI(TAG, "Zigbee stack initialized");
    //------------------ Basic cluster ------------------
    esp_zb_basic_cluster_cfg_t basic_cluster_cfg = {
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
    };
    esp_zb_attribute_list_t *esp_zb_basic_cluster = esp_zb_basic_cluster_create(&basic_cluster_cfg);

    uint8_t ApplicationVersion = 1;
    uint8_t StackVersion = 1;
    uint8_t HWVersion = 1;

    const esp_app_desc_t *app_desc = esp_app_get_description();
    uint8_t SWBuildID[35];
    snprintf((char *)SWBuildID, sizeof(SWBuildID), "%c%s", strlen(app_desc->version), app_desc->version);

    char zigbee_date_code[50];
    snprintf(zigbee_date_code + 1, sizeof(zigbee_date_code), "%s", BUILD_TIME);
    zigbee_date_code[0] = strlen(BUILD_TIME);

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

    // ---------------------- Meter identification cluster ----------------------
    // esp_zb_attribute_list_t *esp_zb_meter_identification_cluster = esp_zb_cluster_create

    // ---------------------- Metering cluster ----------------------
    uint8_t formating = (1 << 7) | (15 << 3) | (3 << 0);
    uint32_t divisor = 1000;
    esp_zb_metering_cluster_cfg_t metering_cluster_cfg = {
        .uint_of_measure = ESP_ZB_ZCL_METERING_UNIT_KW_KWH_BINARY,
        .metering_device_type = ESP_ZB_ZCL_METERING_ELECTRIC_METERING,
        //
        .summation_formatting = formating,
    };
    esp_zb_attribute_list_t *esp_zb_metering_cluster = esp_zb_metering_cluster_create(&metering_cluster_cfg);

    for (esp_zb_attribute_list_t *temp = esp_zb_metering_cluster; temp != NULL; temp = temp->next)
    {
        // ESP_LOGI(TAG, "Metering attr created: att: %x %x", temp->attribute.id, temp->attribute.access);
        if (temp->attribute.id == 0)
        {
            temp->attribute.access = ESP_ZB_ZCL_ATTR_ACCESS_REPORTING;
        }
    }

    esp_zb_cluster_add_attr(esp_zb_metering_cluster, ESP_ZB_ZCL_CLUSTER_ID_METERING, ESP_ZB_ZCL_ATTR_METERING_DEMAND_FORMATTING_ID, ESP_ZB_ZCL_ATTR_TYPE_8BITMAP, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY, &formating);
    esp_zb_cluster_add_attr(esp_zb_metering_cluster, ESP_ZB_ZCL_CLUSTER_ID_METERING, ESP_ZB_ZCL_ATTR_METERING_HISTORICAL_CONSUMPTION_FORMATTING_ID, ESP_ZB_ZCL_ATTR_TYPE_8BITMAP, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY, &formating);
    esp_zb_cluster_add_attr(esp_zb_metering_cluster, ESP_ZB_ZCL_CLUSTER_ID_METERING, ESP_ZB_ZCL_ATTR_METERING_UNIT_OF_MEASURE_ID, ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY, &metering_cluster_cfg.uint_of_measure);
    esp_zb_cluster_add_attr(esp_zb_metering_cluster, ESP_ZB_ZCL_CLUSTER_ID_METERING, ESP_ZB_ZCL_ATTR_METERING_DIVISOR_ID, ESP_ZB_ZCL_ATTR_TYPE_U24, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY, &divisor);

    //------------------ Electrical Measurement cluster ------------------
    esp_zb_electrical_meas_cluster_cfg_t electrical_meas_cluster_cfg = {
        .measured_type = ESP_ZB_ZCL_ELECTRICAL_MEASUREMENT_APPARENT_MEASUREMENT,
    };
    esp_zb_attribute_list_t *esp_zb_electrical_meas_cluster = esp_zb_electrical_meas_cluster_create(&electrical_meas_cluster_cfg);

    // ------------------ TICMeter cluster ------------------
    esp_zb_attribute_list_t *esp_zb_ticmeter_cluster = esp_zb_zcl_attr_list_create(TICMETER_CLUSTER_ID);
    esp_zb_cluster_add_attr(esp_zb_ticmeter_cluster, TICMETER_CLUSTER_ID, 0x0018, ESP_ZB_ZCL_ATTR_TYPE_U8, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY, &ApplicationVersion);

    // ------------------ Add attributes ------------------
    for (int i = 0; i < LinkyLabelListSize; i++)
    {
        if (LinkyLabelList[i].zb_access == 0)
        {
            continue;
        }

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
            if (LinkyLabelList[i].device_class == ENERGY && *(uint32_t *)LinkyLabelList[i].data == 0)
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
        case STRING:
        {
            if (strnlen((char *)LinkyLabelList[i].data, LinkyLabelList[i].size) == 0)
            {
                continue;
            }
            break;
        }
        default:
            break;
        };

        char str_value[100];
        switch (LinkyLabelList[i].type)
        {
        case UINT8:
            sprintf(str_value, "%u", *(uint8_t *)LinkyLabelList[i].data);
            break;
        case UINT16:
            sprintf(str_value, "%u", *(uint16_t *)LinkyLabelList[i].data);
            break;
        case UINT32:
            sprintf(str_value, "%lu", *(uint32_t *)LinkyLabelList[i].data);
            break;
        case UINT64:
            sprintf(str_value, "%llu", *(uint64_t *)LinkyLabelList[i].data);
            break;
        case STRING:
            sprintf(str_value, "%s", (char *)LinkyLabelList[i].data);
            char *str = (char *)LinkyLabelList[i].data;
            char temp[100];
            memcpy(temp + 1, str, LinkyLabelList[i].size + 1);
            temp[0] = strlen(temp + 1);
            if (temp[0] > LinkyLabelList[i].size)
            {
                temp[0] = LinkyLabelList[i].size;
            }
            temp[LinkyLabelList[i].size + 2] = '\0';
            memcpy(LinkyLabelList[i].data, temp, LinkyLabelList[i].size + 1);
            // ESP_LOG_BUFFER_HEXDUMP(TAG, LinkyLabelList[i].data, LinkyLabelList[i].size + 2, ESP_LOG_INFO);
            break;
        default:
            ESP_LOGE(TAG, "%s : Unknown type", LinkyLabelList[i].label);
            return;
            break;
        }

        ESP_LOGI(TAG, "Adding %s : Cluster: %x, attribute: %x, zbtype: %x, value: %s", LinkyLabelList[i].label, LinkyLabelList[i].clusterID, LinkyLabelList[i].attributeID, LinkyLabelList[i].zb_type, str_value);

        switch (LinkyLabelList[i].clusterID)
        {
        case ESP_ZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT:
        {
            esp_zb_electrical_meas_cluster_add_attr(esp_zb_electrical_meas_cluster, LinkyLabelList[i].attributeID, LinkyLabelList[i].data);
            break;
        }
        case ESP_ZB_ZCL_CLUSTER_ID_METERING:
        {
            esp_zb_cluster_add_attr(esp_zb_metering_cluster, ESP_ZB_ZCL_CLUSTER_ID_METERING, LinkyLabelList[i].attributeID, LinkyLabelList[i].zb_type, LinkyLabelList[i].zb_access, LinkyLabelList[i].data);
            break;
        }
        case TICMETER_CLUSTER_ID:
        {
            esp_zb_custom_cluster_add_custom_attr(esp_zb_ticmeter_cluster, LinkyLabelList[i].attributeID, LinkyLabelList[i].zb_type, LinkyLabelList[i].zb_access, LinkyLabelList[i].data);
            break;
        }
        default:
            break;
        }
    }

    //------------------ Cluster list ------------------
    esp_zb_cluster_list_t *esp_zb_cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(esp_zb_cluster_list, esp_zb_identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_electrical_meas_cluster(esp_zb_cluster_list, esp_zb_electrical_meas_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_metering_cluster(esp_zb_cluster_list, esp_zb_metering_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_custom_cluster(esp_zb_cluster_list, esp_zb_ticmeter_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    // esp_zb_cluster_list_add_custom_cluster(esp_zb_cluster_list, esp_zb_meter_custom_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

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
char string_buffer[100];

uint64_t temp = 150;
uint64_t zigbee_summation_delivered = 0;
uint8_t zigbee_send(LinkyData *data)
{
    if (config_values.zigbee.state != ZIGBEE_PAIRED)
    {
        ESP_LOGE(TAG, "Zigbee not paired");
        return 1;
    }
    zigbee_summation_delivered = 0;
    for (int i = 0; i < LinkyLabelListSize; i++)
    {
        ESP_LOGD(TAG, "check %s %d", LinkyLabelList[i].label, i);
        if (LinkyLabelList[i].mode != linky_mode && LinkyLabelList[i].mode != ANY)
        {
            continue;
        }
        if (LinkyLabelList[i].data == NULL)
        {
            continue;
        }
        if (LinkyLabelList[i].clusterID == 0 || LinkyLabelList[i].zb_access == 0)
        {
            continue;
        }

        // if (LinkyLabelList[i].clusterID == TICMETER_CLUSTER_ID) // TODO:
        // {
        //     ESP_LOGW(TAG, "Skip %s cluster", LinkyLabelList[i].label);
        //     continue;
        // }

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
            if (LinkyLabelList[i].device_class == ENERGY && *(uint32_t *)LinkyLabelList[i].data == 0)
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
        case STRING:
        {
            if (strnlen((char *)LinkyLabelList[i].data, LinkyLabelList[i].size) == 0)
            {
                continue;
            }
            char *str = (char *)LinkyLabelList[i].data;
            char temp[100];
            memcpy(temp + 1, str, LinkyLabelList[i].size + 1);
            temp[0] = strlen(temp + 1);
            if (temp[0] > LinkyLabelList[i].size)
            {
                temp[0] = LinkyLabelList[i].size;
            }
            temp[LinkyLabelList[i].size + 2] = '\0';
            memcpy(LinkyLabelList[i].data, temp, LinkyLabelList[i].size + 1);
            // ESP_LOG_BUFFER_HEXDUMP(TAG, LinkyLabelList[i].data, LinkyLabelList[i].size + 2, ESP_LOG_INFO);
            break;
        }
        default:
            break;
        };

        if (LinkyLabelList[i].device_class == ENERGY)
        {
            zigbee_summation_delivered += *(uint32_t *)LinkyLabelList[i].data;
        }

        ESP_LOGD(TAG, "Send %s", LinkyLabelList[i].label);

        esp_zb_zcl_status_t status = ESP_ZB_ZCL_STATUS_SUCCESS;
        if (LinkyLabelList[i].zb_access == ESP_ZB_ZCL_ATTR_ACCESS_REPORTING)
        {
            ESP_LOGI(TAG, "Repporting cluster: 0x%x, attribute: 0x%x, name: %s, value: %llu", LinkyLabelList[i].clusterID, LinkyLabelList[i].attributeID, LinkyLabelList[i].label, *(uint64_t *)LinkyLabelList[i].data);
            uint8_t size = LinkyLabelList[i].type;
            if (size == STRING)
            {
                size = LinkyLabelList[i].size + 1;
            }
            zigbee_report_attribute(LINKY_TIC_ENDPOINT, LinkyLabelList[i].clusterID, LinkyLabelList[i].attributeID, LinkyLabelList[i].data, size);
        }
        else
        {
            status = esp_zb_zcl_set_attribute_val(LINKY_TIC_ENDPOINT, LinkyLabelList[i].clusterID, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, LinkyLabelList[i].attributeID, LinkyLabelList[i].data, false);
            ESP_LOGI(TAG, "Set attribute cluster: Status: 0x%X 0x%x, attribute: 0x%x, name: %s, value: %lu", status, LinkyLabelList[i].clusterID, LinkyLabelList[i].attributeID, LinkyLabelList[i].label, *(uint32_t *)LinkyLabelList[i].data);
        }
    }
    // zigbee_report_attribute(LINKY_TIC_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_METERING, ESP_ZB_ZCL_ATTR_METERING_CURRENT_SUMMATION_DELIVERED_ID, &zigbee_summation_delivered, sizeof(zigbee_summation_delivered));

    return 0;
}