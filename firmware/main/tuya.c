/**
 * @file tuya.cpp
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
#include "tuya.h"
#include "config.h"

#include "tuya_log.h"
#include "tuya_iot.h"
#include "cJSON.h"
#include "qrcode.h"
#include "wifi.h"
#include "esp_ota_ops.h"

/* BLE */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "bleprph.h"

#include "tuya_ble_service.h"
#include "tuya_log.h"
#include "MultiTimer.h"
/*==============================================================================
 Local Define
===============================================================================*/

#define TAG "TUYA"

/*==============================================================================
 Local Macro
===============================================================================*/
#define STATE_ID2STR(S) \
    ((S) == STATE_IDLE ? "STATE_IDLE" : ((S) == STATE_START ? "STATE_START" : ((S) == STATE_DATA_LOAD ? "STATE_DATA_LOAD" : ((S) == STATE_TOKEN_PENDING ? "STATE_TOKEN_PENDING" : ((S) == STATE_ACTIVATING ? "STATE_ACTIVATING" : ((S) == STATE_STARTUP_UPDATE ? "STATE_STARTUP_UPDATE" : ((S) == STATE_MQTT_CONNECT_START ? "STATE_MQTT_CONNECT_START" : ((S) == STATE_MQTT_CONNECTING ? "STATE_MQTT_CONNECTING" : ((S) == STATE_MQTT_RECONNECT ? "STATE_MQTT_RECONNECT" : ((S) == STATE_MQTT_YIELD ? "STATE_MQTT_YIELD" : ((S) == STATE_RESTART ? "STATE_RESTART" : ((S) == STATE_RESET ? "STATE_RESET" : ((S) == STATE_EXIT ? "STATE_EXIT" : "Unknown")))))))))))))
/*==============================================================================
 Local Type
===============================================================================*/
typedef enum
{
    STATE_IDLE,
    STATE_START,
    STATE_DATA_LOAD,
    STATE_TOKEN_PENDING,
    STATE_ACTIVATING,
    STATE_STARTUP_UPDATE,
    STATE_MQTT_CONNECT_START,
    STATE_MQTT_CONNECTING,
    STATE_MQTT_RECONNECT,
    STATE_MQTT_YIELD,
    STATE_RESTART,
    STATE_RESET,
    STATE_STOP,
    STATE_EXIT,
} tuya_run_state_t;
/*==============================================================================
 Local Function Declaration
===============================================================================*/

static void tuya_qrcode_print(const char *productkey, const char *uuid);
static void tuya_user_event_handler_on(tuya_iot_client_t *client, tuya_event_msg_t *event);
static void tuya_iot_dp_download(tuya_iot_client_t *client, const char *json_dps);
static void tuya_link_app_task(void *pvParameters);
static void tuya_send_callback(int result, void *user_data);
static int bleprph_gap_event(struct ble_gap_event *event, void *arg);
void ble_store_config_init(void);
void ble_netcfg_task(void *arg);
/*==============================================================================
Public Variable
===============================================================================*/
TaskHandle_t tuyaTaskHandle = NULL;
TaskHandle_t tuya_ble_pairing_task_handle = NULL;

/*==============================================================================
 Local Variable
===============================================================================*/
static tuya_iot_client_t client;
static tuya_event_id_t lastEvent = TUYA_EVENT_RESET;
static uint8_t newEvent = 0;
static uint8_t own_addr_type;
static tuya_binding_info_t tuya_binding_info;
/*==============================================================================
Function Implementation
===============================================================================*/

static void tuya_qrcode_print(const char *productkey, const char *uuid)
{
    ESP_LOGI(TAG, "https://smartapp.tuya.com/s/p?p=%s&uuid=%s&v=2.0", productkey, uuid);

    char urlbuf[255];
    snprintf(urlbuf, sizeof(urlbuf), "https://smartapp.tuya.com/s/p?p=%s&uuid=%s&v=2.0", productkey, uuid);
    qrcode_display(urlbuf);

    ESP_LOGI(TAG, "(Use this URL to generate a static QR code for the Tuya APP scan code binding)");
}

static void tuya_iot_dp_download(tuya_iot_client_t *client, const char *json_dps)
{
    ESP_LOGI(TAG, "Data point download value:%s", json_dps);

    /* Parsing json string to cJSON object */
    cJSON *dps = cJSON_Parse(json_dps);
    if (dps == NULL)
    {
        ESP_LOGI(TAG, "JSON parsing error, exit!");
        return;
    }

    // /* Process dp data */
    // cJSON *switch_obj = cJSON_GetObjectItem(dps, SWITCH_DP_ID_KEY);
    // if (cJSON_IsTrue(switch_obj))
    // {
    //     hardware_switch_set(true);
    // }
    // else if (cJSON_IsFalse(switch_obj))
    // {
    //     hardware_switch_set(false);
    // }

    /* relese cJSON DPS object */
    cJSON_Delete(dps);

    /* Report the received data to synchronize the switch status. */
    tuya_iot_dp_report_json(client, json_dps);
}

static void tuya_user_event_handler_on(tuya_iot_client_t *client, tuya_event_msg_t *event)
{
    ESP_LOGI(TAG, "TUYA_EVENT: %s", EVENT_ID2STR(event->id));
    switch (event->id)
    {
    case TUYA_EVENT_BIND_START:
        tuya_qrcode_print(client->config.productkey, client->config.uuid);
        break;

    case TUYA_EVENT_MQTT_CONNECTED:
        ESP_LOGI(TAG, "Device MQTT Connected!");
        break;

    case TUYA_EVENT_DP_RECEIVE:
    {
        ESP_LOGI(TAG, "TUYA_EVENT_DP_RECEIVE");
        break;
    }
    case TUYA_EVENT_RESET:
        ESP_LOGI(TAG, "Tuya unbined");
        config_values.tuya.pairing_state = TUYA_WIFI_CONNECTING;
        config_write();
        break;
    case TUYA_EVENT_BIND_TOKEN_ON:
        // start of binding

        break;
    case TUYA_EVENT_ACTIVATE_SUCCESSED:
        ESP_LOGI(TAG, "Tuya binded");
        config_values.tuya.pairing_state = TUYA_PAIRED;

        config_write();
        break;
    default:
        break;
    }
    newEvent = 1;
    lastEvent = event->id;
}

static void tuya_link_app_task(void *pvParameters)
{
    int ret = OPRT_OK;
    const esp_app_desc_t *app_desc = esp_app_get_description();

    const tuya_iot_config_t tuya_config = {
        .productkey = config_values.tuya.product_id,
        .uuid = config_values.tuya.device_uuid,
        .authkey = config_values.tuya.device_auth,
        .software_ver = app_desc->version,
        .modules = NULL,
        .skill_param = NULL,
        .storage_namespace = "tuya_kv",
        .firmware_key = NULL,
        .event_handler = tuya_user_event_handler_on,
    };

    /* Initialize Tuya device configuration */
    ret = tuya_iot_init(&client, &tuya_config);

    if (config_values.tuya.pairing_state == TUYA_WIFI_CONNECTING)
    {
        // client.is_activated = true;
        // client.binding = &tuya_binding_info;
    }

    assert(ret == OPRT_OK);

    /* Start tuya iot task */
    tuya_iot_start(&client);
    while (1)
    {
        /* Loop to receive packets, and handles client keepalive */
        tuya_iot_yield(&client);
        // vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void tuya_init()
{
    ESP_LOGI(TAG, "Tuya init");
    xTaskCreate(tuya_link_app_task, "tuya_link", 1024 * 6, NULL, 4, &tuyaTaskHandle);
}

static void tuya_send_callback(int result, void *user_data)
{
    uint8_t *sendComplete = (uint8_t *)user_data;
    *sendComplete = 1;
}

uint8_t tuya_send_data(LinkyData *linky)
{
    // tuya_iot_reconnect(&client);
    ESP_LOGI(TAG, "Send data to tuya");
    // DynamicJsonDocument device(1024);
    cJSON *jsonObject = cJSON_CreateObject(); // Create the root object

    for (int i = 0; i < LinkyLabelListSize; i++)
    {
        if (LinkyLabelList[i].id < 101)
        {
            continue; // dont send data for label < 101 : they are not used by tuya
        }

        // json
        char strId[5];
        snprintf(strId, sizeof(strId), "%d", LinkyLabelList[i].id);

        switch (LinkyLabelList[i].type)
        {
        case UINT8:
        {
            uint8_t *value = (uint8_t *)LinkyLabelList[i].data;
            if (value == NULL || *value == UINT8_MAX)
                continue;
            // device[strId] = *value;
            cJSON_AddNumberToObject(jsonObject, strId, *value);
            break;
        }
        case UINT16:
        {
            uint16_t *value = (uint16_t *)LinkyLabelList[i].data;
            if (value == NULL || *value == UINT16_MAX)
                continue;
            // device[strId] = *value;
            cJSON_AddNumberToObject(jsonObject, strId, *value);
            break;
        }
        case UINT32:
        {
            uint32_t *value = (uint32_t *)LinkyLabelList[i].data;
            if (value == NULL || *value == UINT32_MAX)
                continue;
            // device[strId] = *value;
            cJSON_AddNumberToObject(jsonObject, strId, *value);
            break;
        }
        case UINT32_TIME:
        {
            uint32_t *value = (uint32_t *)LinkyLabelList[i].data;
            if (value == NULL || *value == UINT32_MAX)
                continue;
            // device[strId] = *value;
            cJSON_AddNumberToObject(jsonObject, strId, *value);
            break;
        }
        case UINT64:
        {
            uint64_t *value = (uint64_t *)LinkyLabelList[i].data;
            if (value == NULL || *value == UINT64_MAX)
                continue;
            // device[strId] = *value;
            cJSON_AddNumberToObject(jsonObject, strId, *value);
            break;
        }
        case STRING:
        {
            char *value = (char *)LinkyLabelList[i].data;
            if (value == NULL || strlen(value) == 0)
                continue;
            // device[strId] = value;
            cJSON_AddStringToObject(jsonObject, strId, value);
            break;
        }
        default:
            break;
        }
    }
    cJSON_AddNumberToObject(jsonObject, "134", MILLIS / 1000 / 60);

    char *json = cJSON_PrintUnformatted(jsonObject); // Convert the json object to string
    cJSON_Delete(jsonObject);                        // Delete the json object

    ESP_LOGI(TAG, "JSON: %s", json);
    uint8_t sendComplete = 0;
    time_t timout = MILLIS + 3000;
    tuya_iot_dp_report_json_with_notify(&client, json, NULL, tuya_send_callback, &sendComplete, 1000);
    while (sendComplete == 0 && MILLIS < timout)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    free(json); // Free the memory

    if (sendComplete == 0)
    {
        ESP_LOGI(TAG, "Send data to tuya timeout");
        return 1;
    }
    ESP_LOGI(TAG, "Send data to tuya OK");
    return 0;
}

void tuya_reset()
{
    ESP_LOGI(TAG, "Reset Tuya");
    config_values.tuya.pairing_state = TUYA_NOT_CONFIGURED;
    tuya_iot_activated_data_remove(&client);
}

void tuya_pairing_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Tuya pairing");

    xTaskCreate(ble_netcfg_task, "ble_netcfg_task", 1024 * 6, NULL, 4, &tuya_ble_pairing_task_handle);

    ESP_LOGI(TAG, "Waiting for tuya BLE pairing");
    while (config_values.tuya.pairing_state != TUYA_WIFI_CONNECTING)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    if (!wifi_connect())
    {
        ESP_LOGE(TAG, "Tuya pairing failed: no wifi");
        vTaskDelete(NULL);
    }

    tuya_reset();
    tuya_init();

    vTaskDelay(5000 / portTICK_PERIOD_MS);

    tuya_endpoint_region_regist_set(tuya_binding_info.region, tuya_binding_info.regist_key);
    tuya_endpoint_update();
    /* DP event send */
    client.event.id = TUYA_EVENT_BIND_TOKEN_ON;
    client.event.type = TUYA_DATE_TYPE_STRING;
    client.event.value.asString = client.binding->token;
    /* Take token go to activate */
    client.nextstate = STATE_ACTIVATING;

    while (config_values.tuya.pairing_state != TUYA_PAIRED)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "Tuya pairing done");
    config_write();

    vTaskDelete(NULL);
}
uint8_t tuya_wait_event(tuya_event_id_t event, uint32_t timeout)
{
    uint32_t timout = MILLIS + timeout;
    while (MILLIS < timout)
    {
        if (newEvent == 1)
        {
            newEvent = 0;
            if (lastEvent == event)
            {
                return 0;
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    return 1;
}

uint8_t tuya_stop()
{
    ESP_LOGI(TAG, "Tuya stop");
    return tuya_iot_stop(&client);
}

uint8_t tuya_restart()
{
    ESP_LOGI(TAG, "Tuya restart");
    return tuya_iot_reconnect(&client);
}

// -------------------------------BLE -----------------------------------------
/**
 * Logs information about a connection to the console.
 */
static void bleprph_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    MODLOG_DFLT(INFO, "handle=%d our_ota_addr_type=%d our_ota_addr=",
                desc->conn_handle, desc->our_ota_addr.type);
    print_addr(desc->our_ota_addr.val);
    MODLOG_DFLT(INFO, " our_id_addr_type=%d our_id_addr=",
                desc->our_id_addr.type);
    print_addr(desc->our_id_addr.val);
    MODLOG_DFLT(INFO, " peer_ota_addr_type=%d peer_ota_addr=",
                desc->peer_ota_addr.type);
    print_addr(desc->peer_ota_addr.val);
    MODLOG_DFLT(INFO, " peer_id_addr_type=%d peer_id_addr=",
                desc->peer_id_addr.type);
    print_addr(desc->peer_id_addr.val);
    MODLOG_DFLT(INFO, " conn_itvl=%d conn_latency=%d supervision_timeout=%d "
                      "encrypted=%d authenticated=%d bonded=%d\n",
                desc->conn_itvl, desc->conn_latency,
                desc->supervision_timeout,
                desc->sec_state.encrypted,
                desc->sec_state.authenticated,
                desc->sec_state.bonded);
}

/**
 * Enables advertising with the following parameters:
 *     o General discoverable mode.
 *     o Undirected connectable mode.
 */
static void
bleprph_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    const char *name;
    int rc;

    /**
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info).
     *     o Advertising tx power.
     *     o Device name.
     *     o 16-bit service UUIDs (alert notifications).
     */

    memset(&fields, 0, sizeof fields);

    /* Advertise two flags:
     *     o Discoverability in forthcoming advertisement (general)
     *     o BLE-only (BR/EDR unsupported).
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                   BLE_HS_ADV_F_BREDR_UNSUP;

    /* Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assigning the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    fields.uuids16 = (ble_uuid16_t[]){
        BLE_UUID16_INIT(GATT_SVR_SVC_ALERT_UUID)};
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0)
    {
        MODLOG_DFLT(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, bleprph_gap_event, NULL);
    if (rc != 0)
    {
        MODLOG_DFLT(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that forms.
 * bleprph uses the same callback for all connections.
 *
 * @param event                 The type of event being signalled.
 * @param ctxt                  Various information pertaining to the event.
 * @param arg                   Application-specified argument; unused by
 *                                  bleprph.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int bleprph_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        MODLOG_DFLT(INFO, "connection %s; status=%d ",
                    event->connect.status == 0 ? "established" : "failed",
                    event->connect.status);
        if (event->connect.status == 0)
        {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            bleprph_print_conn_desc(&desc);
        }
        MODLOG_DFLT(INFO, "\n");

        if (event->connect.status != 0)
        {
            /* Connection failed; resume advertising. */
            bleprph_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        MODLOG_DFLT(INFO, "disconnect; reason=%d ", event->disconnect.reason);
        bleprph_print_conn_desc(&event->disconnect.conn);
        MODLOG_DFLT(INFO, "\n");

        /* Connection terminated; resume advertising. */
        bleprph_advertise();
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        /* The central has updated the connection parameters. */
        MODLOG_DFLT(INFO, "connection updated; status=%d ",
                    event->conn_update.status);
        rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
        assert(rc == 0);
        bleprph_print_conn_desc(&desc);
        MODLOG_DFLT(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        MODLOG_DFLT(INFO, "advertise complete; reason=%d",
                    event->adv_complete.reason);
#if !CONFIG_EXAMPLE_EXTENDED_ADV
        bleprph_advertise();
#endif
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /* Encryption has been enabled or disabled for this connection. */
        MODLOG_DFLT(INFO, "encryption change event; status=%d ",
                    event->enc_change.status);
        rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
        assert(rc == 0);
        bleprph_print_conn_desc(&desc);
        MODLOG_DFLT(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        MODLOG_DFLT(INFO, "subscribe event; conn_handle=%d attr_handle=%d "
                          "reason=%d prevn=%d curn=%d previ=%d curi=%d\n",
                    event->subscribe.conn_handle,
                    event->subscribe.attr_handle,
                    event->subscribe.reason,
                    event->subscribe.prev_notify,
                    event->subscribe.cur_notify,
                    event->subscribe.prev_indicate,
                    event->subscribe.cur_indicate);
        return 0;

    case BLE_GAP_EVENT_MTU:
        MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* We already have a bond with the peer, but it is attempting to
         * establish a new secure link.  This app sacrifices security for
         * convenience: just throw away the old bond and accept the new link.
         */

        /* Delete the old bond. */
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        assert(rc == 0);
        ble_store_util_delete_peer(&desc.peer_id_addr);

        /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
         * continue with the pairing operation.
         */
        return BLE_GAP_REPEAT_PAIRING_RETRY;

    case BLE_GAP_EVENT_PASSKEY_ACTION:
        ESP_LOGI(TAG, "PASSKEY_ACTION_EVENT started \n");
        struct ble_sm_io pkey = {0};
        int key = 0;

        if (event->passkey.params.action == BLE_SM_IOACT_DISP)
        {
            pkey.action = event->passkey.params.action;
            pkey.passkey = 123456; // This is the passkey to be entered on peer
            ESP_LOGI(TAG, "Enter passkey %ld on the peer side", pkey.passkey);
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(TAG, "ble_sm_inject_io result: %d\n", rc);
        }
        else if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP)
        {
            ESP_LOGI(TAG, "Passkey on device's display: %ld", event->passkey.params.numcmp);
            ESP_LOGI(TAG, "Accept or reject the passkey through console in this format -> key Y or key N");
            pkey.action = event->passkey.params.action;
            if (scli_receive_key(&key))
            {
                pkey.numcmp_accept = key;
            }
            else
            {
                pkey.numcmp_accept = 0;
                ESP_LOGE(TAG, "Timeout! Rejecting the key");
            }
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(TAG, "ble_sm_inject_io result: %d\n", rc);
        }
        else if (event->passkey.params.action == BLE_SM_IOACT_OOB)
        {
            static uint8_t tem_oob[16] = {0};
            pkey.action = event->passkey.params.action;
            for (int i = 0; i < 16; i++)
            {
                pkey.oob[i] = tem_oob[i];
            }
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(TAG, "ble_sm_inject_io result: %d\n", rc);
        }
        else if (event->passkey.params.action == BLE_SM_IOACT_INPUT)
        {
            ESP_LOGI(TAG, "Enter the passkey through console in this format-> key 123456");
            pkey.action = event->passkey.params.action;
            if (scli_receive_key(&key))
            {
                pkey.passkey = key;
            }
            else
            {
                pkey.passkey = 0;
                ESP_LOGE(TAG, "Timeout! Passing 0 as the key");
            }
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(TAG, "ble_sm_inject_io result: %d\n", rc);
        }
        return 0;
    }

    return 0;
}

static void bleprph_on_sync(void)
{
    int rc;
    /* Make sure we have proper identity address set (public preferred) */
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0)
    {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }

    /* Printing ADDR */
    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);

    MODLOG_DFLT(INFO, "Device Address: ");
    print_addr(addr_val);
    MODLOG_DFLT(INFO, "\n");
    /* Begin advertising. */
    bleprph_advertise();
}

void bleprph_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}

void ble_token_get_cb(wifi_info_t wifi_info, tuya_binding_info_t binding_info)
{
    ESP_LOGI(TAG, "get wifi info");
    ESP_LOGI(TAG, "ssid: %s", wifi_info.ssid);
    ESP_LOGI(TAG, "Password: %s", wifi_info.pwd);
    ESP_LOGI(TAG, "token: %s", binding_info.token);
    tuya_binding_info = binding_info;
    // save wifi info
    strncpy(config_values.ssid, (char *)wifi_info.ssid, sizeof(config_values.ssid));
    strncpy(config_values.password, (char *)wifi_info.pwd, sizeof(config_values.password));

    config_values.tuya.pairing_state = TUYA_WIFI_CONNECTING;

    if (tuya_ble_pairing_task_handle != NULL)
    {
        vTaskDelete(tuya_ble_pairing_task_handle);
        tuya_ble_pairing_task_handle = NULL;
    }
    return;
}

void ble_netcfg_task(void *arg)
{
    OPERATE_RET rt = OPRT_OK;
    tuya_ble_service_init_params_t init_params = {
        .uuid = (uint8_t *)config_values.tuya.device_uuid,
        .auth_key = (uint8_t *)config_values.tuya.device_auth,
        .pid = (uint8_t *)config_values.tuya.product_id,
    };

    TUYA_CALL_ERR_LOG(tuya_ble_service_start(&init_params, ble_token_get_cb));
    MultiTimerInstall(xTaskGetTickCount);

    for (;;)
    {
        ble_service_loop();
        MultiTimerYield();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}