#include "http.h"
#include "wifi.h"
#include "config.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "tuya.h"
#include "mqtt.h"
#include "mqtt_bind.h"

static const char *TAG = "HTTP"; // TAG for debug
#define INDEX_HTML_PATH "/spiffs/index.html"

#define LOCAL_IP "http://4.3.2.1"

typedef enum
{
    WIFI_CONNECT,
    WIFI_CHECK,
    WIFI_PING,
    MQTT_CONNECT,
    MQTT_PUBLISH,
    TUYA_CONNECT,
    NO_TEST = 0xFF
} test_t;

static test_t current_test = NO_TEST;

void reboot_task(void *pvParameter)
{
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    esp_restart();
}
void urldecode(char *dst, const char *src)
{
    char a, b;
    while (*src)
    {
        if ((*src == '%') && ((a = src[1]) && (b = src[2])) && (isxdigit(a) && isxdigit(b)))
        {
            if (a >= 'a')
                a -= 'a' - 'A';
            if (a >= 'A')
                a -= ('A' - 10);
            else
                a -= '0';
            if (b >= 'a')
                b -= 'a' - 'A';
            if (b >= 'A')
                b -= ('A' - 10);
            else
                b -= '0';
            *dst++ = 16 * a + b;
            src += 3;
        }
        else if (*src == '+')
        {
            *dst++ = ' ';
            src++;
        }
        else
        {
            *dst++ = *src++;
        }
    }
    *dst++ = '\0';
}

esp_err_t send_web_page(httpd_req_t *req)
{
    ESP_LOGI(TAG, "send_web_page: %s", req->uri);

    struct stat st;
    char url[100] = {0};
    if (strcmp(req->uri, "/") == 0)
    {
        strcat(url, INDEX_HTML_PATH);
    }
    else
    {
        strcat(url, "/spiffs");
        strcat(url, req->uri);
    }
    if (stat(url, &st))
    {
        ESP_LOGE(TAG, "%s not found", url);
        esp_err_t ret = httpd_resp_send_404(req);
        return ret;
    }

    // ESP_LOGI(TAG, "Allocating memory %ld bytes for %s", st.st_size, url);
    char *data = (char *)malloc(st.st_size);
    memset((void *)data, 0, st.st_size);
    FILE *fp = fopen(url, "r");
    if (fread(data, st.st_size, 1, fp) == 0)
    {
        ESP_LOGE(TAG, "fread failed");
    }
    fclose(fp);

    int response;

    // find the file extension
    char *ext = strrchr(url, '.');
    if (ext == NULL)
    {
        ESP_LOGE(TAG, "Extension not found");
        return ESP_FAIL;
    }
    // ESP_LOGI(TAG, "Extension: %s", ext);

    // check jpg
    if (strcmp(ext, ".jpg") == 0)
    {
        httpd_resp_set_type(req, "image/jpeg");
    }
    else if (strcmp(ext, ".css") == 0)
    {
        httpd_resp_set_type(req, "text/css");
    }
    else if (strcmp(ext, ".js") == 0)
    {
        httpd_resp_set_type(req, "application/javascript");
    }
    else if (strcmp(ext, ".png") == 0)
    {
        httpd_resp_set_type(req, "image/png");
    }
    else if (strcmp(ext, ".ico") == 0)
    {
        httpd_resp_set_type(req, "image/x-icon");
    }
    else if (strcmp(ext, ".pem") == 0)
    {
        httpd_resp_set_status(req, "403 Forbidden");
        httpd_resp_send(req, "Forbidden", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    else
    {
        httpd_resp_set_type(req, "text/html");
    }

    response = httpd_resp_send(req, data, st.st_size);
    free(data);
    return response;
}

// HTTP Error (404) Handler - Redirects all requests to the root page
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    // Set status
    httpd_resp_set_status(req, "302 Temporary Redirect");
    // Redirect to the "/" root directory
    httpd_resp_set_hdr(req, "Location", LOCAL_IP);
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t get_req_handler(httpd_req_t *req)
{
    return send_web_page(req);
}

esp_err_t get_req_204_handler(httpd_req_t *req)
{
    // Set status
    httpd_resp_set_status(req, "302 Temporary Redirect");
    // Redirect to the "/" root directory
    httpd_resp_set_hdr(req, "Location", LOCAL_IP);
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t get_req_404_handler(httpd_req_t *req)
{
    // Set status
    httpd_resp_set_status(req, "404 Not Found");
    return ESP_OK;
}

esp_err_t get_req_logout_handler(httpd_req_t *req)
{
    // http://logout.net
    // Set status
    httpd_resp_set_status(req, "302 Temporary Redirect");
    // Redirect to the "/" root directory
    httpd_resp_set_hdr(req, "Location", "http://logout.net");
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t get_req_redirect_handler(httpd_req_t *req)
{
    // Set status
    httpd_resp_set_status(req, "302 Temporary Redirect");
    // Redirect to the "/" root directory
    httpd_resp_set_hdr(req, "Location", LOCAL_IP);
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t get_req_200_handler(httpd_req_t *req)
{
    // Set status
    httpd_resp_set_status(req, "200 OK");
    return ESP_OK;
}

esp_err_t save_config_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "URL: %s, len: %d", req->uri, req->content_len);
    char *buf = (char *)malloc(req->content_len + 1);
    if (buf == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for the request");
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "Internal Server Error", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    int ret, remaining = req->content_len;

    while (remaining > 0)
    {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf,
                                  MIN(remaining, req->content_len + 1))) <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ESP_FAIL;
        }
        remaining -= ret;
    }
    // Null terminate the buffer
    buf[req->content_len] = 0;

    ESP_LOGI(TAG, "Received data: %s", buf);

    cJSON *jsonObject = cJSON_Parse(buf);
    if (jsonObject == NULL)
    {
        ESP_LOGE(TAG, "Failed to parse JSON");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Bad Request", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    uint8_t tuya_edited = 0;
    cJSON *item = cJSON_GetObjectItem(jsonObject, "wifi-ssid");
    if (item != NULL)
    {
        strncpy(config_values.ssid, item->valuestring, sizeof(config_values.ssid));
        ESP_LOGI(TAG, "SSID: %s", config_values.ssid);
    }
    item = cJSON_GetObjectItem(jsonObject, "wifi-password");
    if (item != NULL)
    {
        strncpy(config_values.password, item->valuestring, sizeof(config_values.password));
        ESP_LOGI(TAG, "Password: %s", config_values.password);
    }
    item = cJSON_GetObjectItem(jsonObject, "linky-mode");
    if (item != NULL)
    {
        config_values.linkyMode = atoi(item->valuestring);
    }
    item = cJSON_GetObjectItem(jsonObject, "server-mode");
    if (item != NULL)
    {
        config_values.mode = atoi(item->valuestring);
        ESP_LOGI(TAG, "Server mode: %d", config_values.mode);
    }
    item = cJSON_GetObjectItem(jsonObject, "web-url");
    if (item != NULL)
    {
        strncpy(config_values.web.host, item->valuestring, sizeof(config_values.web.host));
    }
    item = cJSON_GetObjectItem(jsonObject, "web-token");
    if (item != NULL)
    {
        strncpy(config_values.web.token, item->valuestring, sizeof(config_values.web.token));
    }
    item = cJSON_GetObjectItem(jsonObject, "web-post");
    if (item != NULL)
    {
        strncpy(config_values.web.postUrl, item->valuestring, sizeof(config_values.web.postUrl));
    }
    item = cJSON_GetObjectItem(jsonObject, "web-config");
    if (item != NULL)
    {
        strncpy(config_values.web.configUrl, item->valuestring, sizeof(config_values.web.configUrl));
    }
    item = cJSON_GetObjectItem(jsonObject, "mqtt-host");
    if (item != NULL)
    {
        strncpy(config_values.mqtt.host, item->valuestring, sizeof(config_values.mqtt.host));
    }
    item = cJSON_GetObjectItem(jsonObject, "mqtt-port");
    if (item != NULL)
    {
        config_values.mqtt.port = atoi(item->valuestring);
    }
    item = cJSON_GetObjectItem(jsonObject, "mqtt-user");
    if (item != NULL)
    {
        strncpy(config_values.mqtt.username, item->valuestring, sizeof(config_values.mqtt.username));
    }
    item = cJSON_GetObjectItem(jsonObject, "mqtt-password");
    if (item != NULL)
    {
        strncpy(config_values.mqtt.password, item->valuestring, sizeof(config_values.mqtt.password));
    }
    item = cJSON_GetObjectItem(jsonObject, "mqtt-topic");
    if (item != NULL)
    {
        strncpy(config_values.mqtt.topic, item->valuestring, sizeof(config_values.mqtt.topic));
    }
    item = cJSON_GetObjectItem(jsonObject, "tuya-device-uuid");
    if (item != NULL)
    {
        strncpy(config_values.tuya.device_uuid, item->valuestring, sizeof(config_values.tuya.device_uuid));
        tuya_edited = 1;
    }
    item = cJSON_GetObjectItem(jsonObject, "tuya-device-auth");
    if (item != NULL)
    {
        strncpy(config_values.tuya.device_auth, item->valuestring, sizeof(config_values.tuya.device_auth));
        tuya_edited = 1;
    }

    item = cJSON_GetObjectItem(jsonObject, "refresh-rate");
    if (item != NULL)
    {
        config_values.refreshRate = atoi(item->valuestring);
        if (config_values.refreshRate < 30)
        {
            config_values.refreshRate = 30;
        }
    }

    cJSON_Delete(jsonObject);
    free(buf);

    if (tuya_edited)
    {
        ESP_LOGI(TAG, "Tuya edited, enabling RW");
        config_rw();
    }

    config_write();

    httpd_resp_set_status(req, "200 OK");
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t get_config_handler(httpd_req_t *req)
{
    cJSON *jsonObject = cJSON_CreateObject();
    cJSON_AddStringToObject(jsonObject, "wifi-ssid", config_values.ssid);
    cJSON_AddNumberToObject(jsonObject, "wifi-password", strnlen(config_values.password, sizeof(config_values.password)));
    cJSON_AddNumberToObject(jsonObject, "linky-mode", config_values.mode);
    cJSON_AddNumberToObject(jsonObject, "server-mode", config_values.mode);
    cJSON_AddStringToObject(jsonObject, "web-url", config_values.web.host);
    cJSON_AddStringToObject(jsonObject, "web-token", config_values.web.token);
    cJSON_AddStringToObject(jsonObject, "web-post", config_values.web.postUrl);
    cJSON_AddStringToObject(jsonObject, "web-config", config_values.web.configUrl);
    cJSON_AddStringToObject(jsonObject, "mqtt-host", config_values.mqtt.host);
    cJSON_AddNumberToObject(jsonObject, "mqtt-port", config_values.mqtt.port);
    cJSON_AddStringToObject(jsonObject, "mqtt-user", config_values.mqtt.username);
    cJSON_AddNumberToObject(jsonObject, "mqtt-password", strnlen(config_values.mqtt.password, sizeof(config_values.mqtt.password)));
    cJSON_AddStringToObject(jsonObject, "mqtt-topic", config_values.mqtt.topic);
    cJSON_AddStringToObject(jsonObject, "tuya-device-uuid", config_values.tuya.device_uuid);
    cJSON_AddNumberToObject(jsonObject, "tuya-device-auth", strnlen(config_values.tuya.device_auth, sizeof(config_values.tuya.device_auth)));
    cJSON_AddNumberToObject(jsonObject, "refresh-rate", config_values.refreshRate);

    char *jsonString = cJSON_PrintUnformatted(jsonObject);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, jsonString, strlen(jsonString));
    free(jsonString);
    cJSON_Delete(jsonObject);
    return ESP_OK;
}

esp_err_t wifi_scan_handler(httpd_req_t *req)
{
    uint16_t ap_num = 0;
    wifi_scan(&ap_num);
    cJSON *jsonObject = cJSON_CreateObject();
    cJSON *jsonArray = cJSON_CreateArray();
    for (int i = 0; i < ap_num; i++)
    {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "ssid", (const char *)wifi_ap_list[i].ssid);
        cJSON_AddNumberToObject(item, "rssi", wifi_ap_list[i].rssi);
        cJSON_AddNumberToObject(item, "channel", wifi_ap_list[i].primary);
        cJSON_AddNumberToObject(item, "auth", wifi_ap_list[i].authmode);
        cJSON_AddItemToArray(jsonArray, item);
    }
    cJSON_AddItemToObject(jsonObject, "ap", jsonArray);
    char *jsonString = cJSON_PrintUnformatted(jsonObject);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, jsonString, strlen(jsonString));
    free(jsonString);
    cJSON_Delete(jsonObject);
    return ESP_OK;
}

const char *str_wifi_error[] = {
    [ESP_ERR_WIFI_MODE] = "Merci de remplir le champ SSID et Mot de passe",
    [ESP_ERR_WIFI_SSID] = "SSID incorrect",
    [ESP_ERR_WIFI_PASSWORD] = "Mot de passe incorrect",
};

esp_err_t test_start_handler(httpd_req_t *req)
{
    static esp_err_t last_wifi_connect = -2;
    char buf[100];
    esp_err_t err = httpd_req_get_url_query_str(req, buf, sizeof(buf));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get the query string");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Bad Request", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    char value[10];
    err = httpd_query_key_value(buf, "id", value, sizeof(value));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get the id");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Bad Request", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    uint32_t id = atoi(value);
    switch (id)
    {
    case WIFI_CONNECT:
        last_wifi_connect = -2;
        httpd_resp_set_status(req, "200 OK");
        httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        wifi_disconnect();
        last_wifi_connect = wifi_connect();
        ESP_LOGI(TAG, "TEST WIFI_CONNECT: %d", last_wifi_connect);

        return ESP_OK;
        break;
    case WIFI_CHECK:

        if (last_wifi_connect == -2)
        {
            // pending
            httpd_resp_set_status(req, "202 Accepted");
            httpd_resp_send(req, "Accepted", HTTPD_RESP_USE_STRLEN);
            return ESP_FAIL;
        }
        if (last_wifi_connect != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to connect to wifi");

            switch (last_wifi_connect)
            {
            case ESP_ERR_WIFI_MODE:
                sprintf(buf, "Merci de remplir le champ SSID et Mot de passe");
                break;
            case ESP_ERR_WIFI_SSID:
                sprintf(buf, "Réseau non trouvé, vérifiez le SSID");
                break;
            case ESP_ERR_WIFI_PASSWORD:
                sprintf(buf, "Mot de passe incorrect");
                break;
            case ESP_FAIL:
                sprintf(buf, "Connexion échouée");
                break;
            default:
                sprintf(buf, "%s (0x%x)", esp_err_to_name(last_wifi_connect), last_wifi_connect);
                break;
            }
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
            return ESP_FAIL;
        }

        httpd_resp_set_status(req, "200 OK");
        httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
        break;
    case WIFI_PING:
    {
        ip_addr_t ip = {
            .type = IPADDR_TYPE_V4,
            .u_addr.ip4.addr = wifi_current_ip.gw.addr,
        };
        uint32_t ping;
        err = wifi_ping(ip, &ping);
        if (err != ESP_OK)
        {
            sprintf(buf, "Ping %s failed.", ip4addr_ntoa(&ip.u_addr.ip4));
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
            return ESP_FAIL;
        }
        httpd_resp_set_status(req, "200 OK");
        httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
        break;
    }
    break;
    case MQTT_CONNECT:
    {
        esp_mqtt_error_type_t type;
        esp_mqtt_connect_return_code_t return_code;

        err = mqtt_test(&type, &return_code);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "MQTT error: type: %d, return_code: %d", type, return_code);
            switch (return_code)
            {
            case MQTT_CONNECTION_REFUSE_PROTOCOL:
                sprintf(buf, "Connexion refusée, mauvais protocole");
                break;
            case MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE:
                sprintf(buf, "Connexion refusée, serveur indisponible");
                break;
            case MQTT_CONNECTION_REFUSE_BAD_USERNAME:
                sprintf(buf, "Connexion refusée, mauvais nom d'utilisateur ou mot de passe");
                break;
            case MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED:
                sprintf(buf, "Connexion refusée, non autorisé");
                break;
            default:
                sprintf(buf, "Connexion refusée, raison inconnue");
                break;
            }
            mqtt_deinit();
            httpd_resp_set_type(req, "text/plain; charset=utf-8");
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
            return ESP_FAIL;
        }

        httpd_resp_set_status(req, "200 OK");
        httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);

        break;
    }
    case MQTT_PUBLISH:
        err = mqtt_prepare_publish(&linky_data);
        if (err == 0)
        {
            ESP_LOGE(TAG, "Failed to prepare message");
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_send(req, "Internal Server Error", HTTPD_RESP_USE_STRLEN);
            mqtt_deinit();
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Waiting for MQTT send done, outbox size: %d", esp_mqtt_client_get_outbox_size(mqtt_client));
        time_t mqtt_send_timeout = MILLIS + 10000;
        while ((mqtt_send_timeout > MILLIS) && /*mqtt_sent_count < mqtt_sensors_count*/ esp_mqtt_client_get_outbox_size(mqtt_client) > 0)
        {
            vTaskDelay(100 / portTICK_PERIOD_MS);
            ESP_LOGD(TAG, "Outbox size: %d", esp_mqtt_client_get_outbox_size(mqtt_client));
        }
        if (esp_mqtt_client_get_outbox_size(mqtt_client) > 0)
        {
            ESP_LOGE(TAG, "Failed to send all messages");
            mqtt_deinit();
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_send(req, "Impossible d'envoyer tous les messages", HTTPD_RESP_USE_STRLEN);
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "All messages sent");
        mqtt_deinit();
        httpd_resp_set_status(req, "200 OK");
        httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);

        return ESP_OK;

        break;

    case TUYA_CONNECT:
        if (!tuya_available())
        {
            ESP_LOGE(TAG, "Tuya device UUID or Auth missing");
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_send(req, "Vous ne possédez pas de clés Tuya", HTTPD_RESP_USE_STRLEN);
            return ESP_FAIL;
        }
        tuya_init();

        time_t tuya_ping_timeout = MILLIS + 7000;
        ESP_LOGI(TAG, "Waiting for Tuya connect event...");
        while (mqtt_bind_state != STATE_MQTT_BIND_TOKEN_WAIT && tuya_ping_timeout > MILLIS)
        {
            ESP_LOGD(TAG, "Tuya state: %d", mqtt_bind_state);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        if (mqtt_bind_state != STATE_MQTT_BIND_TOKEN_WAIT)
        {
            ESP_LOGE(TAG, "Failed to connect to Tuya");
            httpd_resp_set_status(req, "500 Internal Server Error");
            snprintf(buf, sizeof(buf), "Impossible de se connecter à Tuya: Etat %d", mqtt_bind_state);
            httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
            tuya_deinit();
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Tuya connected");
        httpd_resp_set_status(req, "200 OK");
        httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
        tuya_deinit();

        return ESP_OK;
        break;
    default:
        ESP_LOGE(TAG, "Unknown test id");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Bad Request", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
        break;
    }
    current_test = id;

    httpd_resp_set_status(req, "200 OK");
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

esp_err_t test_status_handler(httpd_req_t *req)
{

    ESP_LOGI(TAG, "URL: %s", req->uri);
    // response ok
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

esp_err_t get_reboot_handler(httpd_req_t *req)
{
    // response ok
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);

    xTaskCreate(&reboot_task, "reboot_task", 2048, NULL, 20, NULL);
    return ESP_OK;
}

struct request_item_t
{
    const char *uri;
    httpd_method_t method;
    esp_err_t (*handler)(httpd_req_t *req);
};

// clang-format off
struct request_item_t requests[] = {
    {"/gen_204",                HTTP_GET,   get_req_204_handler},
    {"/generate_204",           HTTP_GET,   get_req_204_handler},
    {"/config",                 HTTP_POST,  save_config_handler},
    {"/config",                 HTTP_GET,   get_config_handler},
    {"/test-start",             HTTP_GET,   test_start_handler},
    {"/reboot",                 HTTP_GET,   get_reboot_handler},
    {"/wifi-scan",              HTTP_GET,   wifi_scan_handler},
    {"/wpad.dat",               HTTP_GET,   get_req_404_handler},
    {"/chat",                   HTTP_GET,   get_req_404_handler},
    {"/connecttest.txt",        HTTP_GET,   get_req_logout_handler},
    {"/redirect",               HTTP_GET,   get_req_redirect_handler},
    {"/hotspot-detect.html",    HTTP_GET,   get_req_redirect_handler},
    {"/canonical.html",         HTTP_GET,   get_req_redirect_handler},
    {"/success.txt",            HTTP_GET,   get_req_200_handler},
    {"/ncsi.txt",               HTTP_GET,   get_req_redirect_handler},

    {"/*",                      HTTP_GET,   get_req_handler},
};

// clang-format on

const uint8_t request_count = sizeof(requests) / sizeof(requests[0]);

httpd_handle_t setup_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 32 * 1024;
    httpd_handle_t server = NULL;

    config.max_open_sockets = 7;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = request_count + 1;
    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
        for (int i = 0; i < request_count; i++)
        {
            httpd_uri_t uri = {
                .uri = requests[i].uri,
                .method = requests[i].method,
                .handler = requests[i].handler,
                .user_ctx = NULL};
            httpd_register_uri_handler(server, &uri);
        }
    }

    return server;
}
