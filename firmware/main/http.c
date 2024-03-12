#include "http.h"
#include "wifi.h"
#include "config.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "tuya.h"

static const char *TAG = "HTTP"; // TAG for debug
#define INDEX_HTML_PATH "/spiffs/index.html"

#define LOCAL_IP "http://4.3.2.1"

typedef enum
{
    NO_TEST,
    WIFI_CONNECT,
    WIFI_PING,
    MQTT_CONNECT,
    MQTT_PUBLISH,

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
    char buf[500];
    int ret, remaining = req->content_len;

    while (remaining > 0)
    {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf,
                                  MIN(remaining, sizeof(buf)))) <= 0)
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

    char decoded[500];
    urldecode(decoded, buf);

    /* Log data received */
    ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
    ESP_LOGI(TAG, "%s", decoded);
    ESP_LOGI(TAG, "====================================");

    // Parse the POST parameters
    char ssid[100] = {0};
    char password[100] = {0};
    char server_mode = 0;
    char web_url[100] = {0};
    char web_post_url[100] = {0};
    char web_config_url[100] = {0};
    char web_token[100] = {0};
    char linky_mode = 0;
    char mqtt_host[100] = {0};
    uint16_t mqtt_port = 0;
    char mqtt_user[100] = {0};
    char mqtt_password[100] = {0};
    char mqtt_topic[100] = {0};
    char mqtt_HA_discovery = 0;

    // wifi-ssid=Test&wifi-password=tedt&server-mode=1&web-url=Tedt&web-token=Tehe&web-tarif=base&web-price-base=&web-price-hc=&web-price-hp=&mqtt-host=&mqtt-port=&mqtt-user=&mqtt-password=
    // parse key value pairs
    char *key = strtok(decoded, "&");
    while (key != NULL)
    {
        char *value = strchr(key, '=');
        if (value != NULL)
        {
            *value = '\0';
            value++;
            if (strcmp(key, "wifi-ssid") == 0)
            {
                strncpy(ssid, value, sizeof(ssid));
            }
            else if (strcmp(key, "wifi-password") == 0)
            {
                strncpy(password, value, sizeof(password));
            }
            else if (strcmp(key, "server-mode") == 0)
            {
                server_mode = atoi(value);
            }
            else if (strcmp(key, "web-url") == 0)
            {
                strncpy(web_url, value, sizeof(web_url));
            }
            else if (strcmp(key, "web-token") == 0)
            {
                strncpy(web_token, value, sizeof(web_token));
            }
            else if (strcmp(key, "web-config") == 0)
            {
                strncpy(web_config_url, value, sizeof(web_config_url));
            }
            else if (strcmp(key, "web-post") == 0)
            {
                strncpy(web_post_url, value, sizeof(web_post_url));
            }
            else if (strcmp(key, "linky-mode") == 0)
            {
                linky_mode = atoi(value);
            }
            else if (strcmp(key, "mqtt-host") == 0)
            {
                strncpy(mqtt_host, value, sizeof(mqtt_host));
            }
            else if (strcmp(key, "mqtt-port") == 0)
            {
                mqtt_port = atoi(value);
            }
            else if (strcmp(key, "mqtt-user") == 0)
            {
                strncpy(mqtt_user, value, sizeof(mqtt_user));
            }
            else if (strcmp(key, "mqtt-password") == 0)
            {
                strncpy(mqtt_password, value, sizeof(mqtt_password));
            }
            else if (strcmp(key, "mqtt-topic") == 0)
            {
                strncpy(mqtt_topic, value, sizeof(mqtt_topic));
            }
            else if (strcmp(key, "mqtt-ha-discovery") == 0)
            {
                mqtt_HA_discovery = atoi(value);
            }
        }
        key = strtok(NULL, "&");
    }
    // save the parameters
    strncpy(config_values.ssid, ssid, sizeof(config_values.ssid));
    strncpy(config_values.password, password, sizeof(config_values.password));

    if (server_mode == MODE_MQTT && mqtt_HA_discovery == 1)
    {
        config_values.mode = MODE_MQTT_HA;
    }
    else
    {
        config_values.mode = (connectivity_t)server_mode;
    }

    strncpy(config_values.web.host, web_url, sizeof(config_values.web.host));
    strncpy(config_values.web.token, web_token, sizeof(config_values.web.token));
    strncpy(config_values.web.configUrl, web_config_url, sizeof(config_values.web.configUrl));
    strncpy(config_values.web.postUrl, web_post_url, sizeof(config_values.web.postUrl));
    if (linky_mode > AUTO)
        linky_mode = AUTO;
    config_values.linkyMode = (linky_mode_t)linky_mode;
    strncpy(config_values.mqtt.host, mqtt_host, sizeof(config_values.mqtt.host));
    config_values.mqtt.port = mqtt_port;
    strncpy(config_values.mqtt.username, mqtt_user, sizeof(config_values.mqtt.username));
    strncpy(config_values.mqtt.password, mqtt_password, sizeof(config_values.mqtt.password));
    strncpy(config_values.mqtt.topic, mqtt_topic, sizeof(config_values.mqtt.topic));

    // //  redirect to the reboot page
    // httpd_resp_set_status(req, "302 Temporary Redirect");
    // httpd_resp_set_hdr(req, "Location", "/reboot.html");
    // httpd_resp_send(req, "Redirect to the reboot page", HTTPD_RESP_USE_STRLEN);
    httpd_resp_set_status(req, "200 OK");

    config_rw();
    config_write();

    // reboot the device
    // xTaskCreate(&reboot_task, "reboot_task", 2048, NULL, 20, NULL);
    return ESP_OK;
}

esp_err_t get_config_handler(httpd_req_t *req)
{
    cJSON *jsonObject = cJSON_CreateObject();
    cJSON_AddStringToObject(jsonObject, "wifi-ssid", config_values.ssid);
    // cJSON_AddStringToObject(jsonObject, "wifi-password", config_values.password);
    cJSON_AddNumberToObject(jsonObject, "linky-mode", config_values.mode);
    cJSON_AddNumberToObject(jsonObject, "server-mode", config_values.mode);
    cJSON_AddStringToObject(jsonObject, "web-url", config_values.web.host);
    cJSON_AddStringToObject(jsonObject, "web-token", config_values.web.token);
    cJSON_AddStringToObject(jsonObject, "web-post", config_values.web.postUrl);
    cJSON_AddStringToObject(jsonObject, "web-config", config_values.web.configUrl);
    cJSON_AddStringToObject(jsonObject, "mqtt-host", config_values.mqtt.host);
    cJSON_AddNumberToObject(jsonObject, "mqtt-port", config_values.mqtt.port);
    cJSON_AddStringToObject(jsonObject, "mqtt-user", config_values.mqtt.username);
    cJSON_AddStringToObject(jsonObject, "mqtt-password", config_values.mqtt.password);
    cJSON_AddStringToObject(jsonObject, "mqtt-topic", config_values.mqtt.topic);
    cJSON_AddStringToObject(jsonObject, "tuya-device-uuid", config_values.tuya.device_uuid);
    cJSON_AddStringToObject(jsonObject, "tuya-device-auth", config_values.tuya.device_auth);

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

esp_err_t test_start_handler(httpd_req_t *req)
{
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
        err = wifi_connect();
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to connect to wifi");
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_send(req, "Internal Server Error", HTTPD_RESP_USE_STRLEN);
            return ESP_FAIL;
        }
        break;
    case WIFI_PING:
        ip_addr_t ip = {
            .type = IPADDR_TYPE_V4,
            .u_addr.ip4.addr = wifi_current_ip.ip.addr,
        };
        wifi_ping(ip);
        break;
    case MQTT_CONNECT:
        break;
    case MQTT_PUBLISH:
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
    {"/save-config",            HTTP_POST,  save_config_handler},
    {"/config",                 HTTP_GET,   get_config_handler},
    {"/test-start",             HTTP_GET,   test_start_handler},
    {"/test-status",            HTTP_GET,   test_status_handler},
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
