#include "http.h"
#include "config.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

static const char *TAG = "HTTP"; // TAG for debug
#define INDEX_HTML_PATH "/spiffs/index.html"

#define LOCAL_IP "http://4.3.2.1"

char response_data[4096];
void reboot_task(void *pvParameter)
{
    vTaskDelay(5000 / portTICK_PERIOD_MS);
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

esp_vfs_spiffs_conf_t conf;
void initi_web_page_buffer(void)
{
    conf.base_path = "/spiffs";
    conf.partition_label = NULL;
    conf.max_files = 5;
    conf.format_if_mount_failed = true;

    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));
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

    ESP_LOGI(TAG, "Allocating memory %ld bytes for %s", st.st_size, url);
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
    ESP_LOGI(TAG, "Extension: %s", ext);

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

    ESP_LOGI(TAG, "Redirecting to root");
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

    ESP_LOGI(TAG, "Redirecting to root");
    return ESP_OK;
}

esp_err_t get_req_404_handler(httpd_req_t *req)
{
    // Set status
    httpd_resp_set_status(req, "404 Not Found");
    // Redirect to the "/" root directory
    // httpd_resp_set_hdr(req, "Location", "/");
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    // httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Redirecting to root");
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

    ESP_LOGI(TAG, "Redirecting to root");
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

    ESP_LOGI(TAG, "Redirecting to root");
    return ESP_OK;
}

esp_err_t get_req_200_handler(httpd_req_t *req)
{
    // Set status
    httpd_resp_set_status(req, "200 OK");
    // Redirect to the "/" root directory
    // httpd_resp_set_hdr(req, "Location", "/");
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    // httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Redirecting to root");
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
                strcpy(ssid, value);
            }
            else if (strcmp(key, "wifi-password") == 0)
            {
                strcpy(password, value);
            }
            else if (strcmp(key, "server-mode") == 0)
            {
                server_mode = atoi(value);
            }
            else if (strcmp(key, "web-url") == 0)
            {
                strcpy(web_url, value);
            }
            else if (strcmp(key, "web-token") == 0)
            {
                strcpy(web_token, value);
            }
            else if (strcmp(key, "web-config") == 0)
            {
                strcpy(web_config_url, value);
            }
            else if (strcmp(key, "web-post") == 0)
            {
                strcpy(web_post_url, value);
            }
            else if (strcmp(key, "linky-mode") == 0)
            {
                linky_mode = atoi(value);
            }
            else if (strcmp(key, "mqtt-host") == 0)
            {
                strcpy(mqtt_host, value);
            }
            else if (strcmp(key, "mqtt-port") == 0)
            {
                mqtt_port = atoi(value);
            }
            else if (strcmp(key, "mqtt-user") == 0)
            {
                strcpy(mqtt_user, value);
            }
            else if (strcmp(key, "mqtt-password") == 0)
            {
                strcpy(mqtt_password, value);
            }
            else if (strcmp(key, "mqtt-topic") == 0)
            {
                strcpy(mqtt_topic, value);
            }
            else if (strcmp(key, "mqtt-ha-discovery") == 0)
            {
                mqtt_HA_discovery = atoi(value);
            }
        }
        key = strtok(NULL, "&");
    }

    // print the parameters
    // ESP_LOGI(TAG, "ssid: %s", ssid);
    // ESP_LOGI(TAG, "password: %s", password);
    // ESP_LOGI(TAG, "server_mode: %d", server_mode);
    // ESP_LOGI(TAG, "web_url: %s", web_url);
    // ESP_LOGI(TAG, "web_token: %s", web_token);
    // ESP_LOGI(TAG, "linky_mode: %s", linky_mode);
    // ESP_LOGI(TAG, "mqtt_host: %s", mqtt_host);
    // ESP_LOGI(TAG, "mqtt_port: %d", mqtt_port);
    // ESP_LOGI(TAG, "mqtt_user: %s", mqtt_user);
    // ESP_LOGI(TAG, "mqtt_password: %s", mqtt_password);
    // ESP_LOGI(TAG, "mqtt_topic: %s", mqtt_topic);

    // save the parameters
    strcpy(config.values.ssid, ssid);
    strcpy(config.values.password, password);

    if (server_mode == MODE_MQTT && mqtt_HA_discovery == 1)
    {
        config.values.mode = MODE_MQTT_HA;
    }
    else
    {
        config.values.mode = (connectivity_t)server_mode;
    }

    strcpy(config.values.web.host, web_url);
    strcpy(config.values.web.token, web_token);
    strcpy(config.values.web.configUrl, web_config_url);
    strcpy(config.values.web.postUrl, web_post_url);
    config.values.linkyMode = (LinkyMode)linky_mode;
    strcpy(config.values.mqtt.host, mqtt_host);
    config.values.mqtt.port = mqtt_port;
    strcpy(config.values.mqtt.username, mqtt_user);
    strcpy(config.values.mqtt.password, mqtt_password);
    strcpy(config.values.mqtt.topic, mqtt_topic);
    config.write();

    //  redirect to the reboot page
    httpd_resp_set_status(req, "302 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/reboot.html");
    httpd_resp_send(req, "Redirect to the reboot page", HTTPD_RESP_USE_STRLEN);

    // reboot the device
    xTaskCreate(&reboot_task, "reboot_task", 2048, NULL, 5, NULL);
    return ESP_OK;
}

esp_err_t get_config_handler(httpd_req_t *req)
{
    cJSON *jsonObject = cJSON_CreateObject();
    cJSON_AddStringToObject(jsonObject, "wifi-ssid", config.values.ssid);
    cJSON_AddStringToObject(jsonObject, "wifi-password", config.values.password);
    cJSON_AddNumberToObject(jsonObject, "linky-mode", config.values.mode);
    cJSON_AddNumberToObject(jsonObject, "server-mode", config.values.mode);
    cJSON_AddStringToObject(jsonObject, "web-url", config.values.web.host);
    cJSON_AddStringToObject(jsonObject, "web-token", config.values.web.token);
    cJSON_AddStringToObject(jsonObject, "web-post", config.values.web.postUrl);
    cJSON_AddStringToObject(jsonObject, "web-config", config.values.web.configUrl);
    cJSON_AddStringToObject(jsonObject, "mqtt-host", config.values.mqtt.host);
    cJSON_AddNumberToObject(jsonObject, "mqtt-port", config.values.mqtt.port);
    cJSON_AddStringToObject(jsonObject, "mqtt-user", config.values.mqtt.username);
    cJSON_AddStringToObject(jsonObject, "mqtt-password", config.values.mqtt.password);
    cJSON_AddStringToObject(jsonObject, "mqtt-topic", config.values.mqtt.topic);
    cJSON_AddStringToObject(jsonObject, "tuya-productID", config.values.tuya.productID);
    cJSON_AddStringToObject(jsonObject, "tuya-deviceUUID", config.values.tuya.deviceUUID);

    char *jsonString = cJSON_PrintUnformatted(jsonObject);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, jsonString, strlen(jsonString));
    free(jsonString);
    cJSON_Delete(jsonObject);
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
