#include "http.h"

static const char *TAG = "HTTP"; // TAG for debug
#define INDEX_HTML_PATH "/spiffs/index.html"
char index_html[4096];
char response_data[4096];

esp_vfs_spiffs_conf_t conf;
void initi_web_page_buffer(void)
{
    conf.base_path = "/spiffs";
    conf.partition_label = NULL;
    conf.max_files = 5;
    conf.format_if_mount_failed = true;

    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));

    memset((void *)index_html, 0, sizeof(index_html));
    struct stat st;
    if (stat(INDEX_HTML_PATH, &st))
    {
        ESP_LOGE(TAG, "index.html not found");
        return;
    }

    FILE *fp = fopen(INDEX_HTML_PATH, "r");
    if (fread(index_html, st.st_size, 1, fp) == 0)
    {
        ESP_LOGE(TAG, "fread failed");
    }
    fclose(fp);
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
    httpd_resp_set_hdr(req, "Location", "/");
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
    httpd_resp_set_hdr(req, "Location", "/");
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

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
    /* Log data received */
    ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
    ESP_LOGI(TAG, "%s", buf);
    ESP_LOGI(TAG, "====================================");

    // Parse the POST parameters
    char ssid[100] = {0};
    char password[100] = {0};
    char server_mode = 0;
    char web_url[100] = {0};
    char web_token[100] = {0};
    char web_tarif[100] = {0};
    float web_price_base = 0;
    float web_price_hc = 0;
    float web_price_hp = 0;
    char mqtt_host[100] = {0};
    uint16_t mqtt_port = 0;
    char mqtt_user[100] = {0};
    char mqtt_password[100] = {0};
    char mqtt_topic[100] = {0};

    char *p = strtok(buf, "&");
    while (p != NULL)
    {
        char *e = strchr(p, '=');
        if (e != NULL)
        {
            *e = '\0';
            char *value = e + 1;
            if (strcmp(p, "ssid") == 0)
            {
                strcpy(ssid, value);
            }
            else if (strcmp(p, "password") == 0)
            {
                strcpy(password, value);
            }
            else if (strcmp(p, "server_mode") == 0)
            {
                server_mode = atoi(value);
            }
            else if (strcmp(p, "web_url") == 0)
            {
                strcpy(web_url, value);
            }
            else if (strcmp(p, "web_token") == 0)
            {
                strcpy(web_token, value);
            }
            else if (strcmp(p, "web_tarif") == 0)
            {
                strcpy(web_tarif, value);
            }
            else if (strcmp(p, "web_price_base") == 0)
            {
                web_price_base = atof(value);
            }
            else if (strcmp(p, "web_price_hc") == 0)
            {
                web_price_hc = atof(value);
            }
            else if (strcmp(p, "web_price_hp") == 0)
            {
                web_price_hp = atof(value);
            }
            else if (strcmp(p, "mqtt_host") == 0)
            {
                strcpy(mqtt_host, value);
            }
            else if (strcmp(p, "mqtt_port") == 0)
            {
                mqtt_port = atoi(value);
            }
            else if (strcmp(p, "mqtt_user") == 0)
            {
                strcpy(mqtt_user, value);
            }
            else if (strcmp(p, "mqtt_password") == 0)
            {
                strcpy(mqtt_password, value);
            }
            else if (strcmp(p, "mqtt_topic") == 0)
            {
                strcpy(mqtt_topic, value);
            }
        }
        p = strtok(NULL, "&");
    }

    // print the parameters
    ESP_LOGI(TAG, "ssid: %s", ssid);
    ESP_LOGI(TAG, "password: %s", password);
    ESP_LOGI(TAG, "server_mode: %d", server_mode);
    ESP_LOGI(TAG, "web_url: %s", web_url);
    ESP_LOGI(TAG, "web_token: %s", web_token);
    ESP_LOGI(TAG, "web_tarif: %s", web_tarif);
    ESP_LOGI(TAG, "web_price_base: %f", web_price_base);
    ESP_LOGI(TAG, "web_price_hc: %f", web_price_hc);
    ESP_LOGI(TAG, "web_price_hp: %f", web_price_hp);
    ESP_LOGI(TAG, "mqtt_host: %s", mqtt_host);
    ESP_LOGI(TAG, "mqtt_port: %d", mqtt_port);
    ESP_LOGI(TAG, "mqtt_user: %s", mqtt_user);
    ESP_LOGI(TAG, "mqtt_password: %s", mqtt_password);
    ESP_LOGI(TAG, "mqtt_topic: %s", mqtt_topic);

    //  redirect to the reboot page
    httpd_resp_set_status(req, "302 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/reboot.html");
    httpd_resp_send(req, "Redirect to the reboot page", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

httpd_uri_t uri_get = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = get_req_handler,
    .user_ctx = NULL};

httpd_uri_t uri_204 = {
    .uri = "/gen_204",
    .method = HTTP_GET,
    .handler = get_req_204_handler,
    .user_ctx = NULL};

httpd_uri_t uri_2044 = {
    .uri = "/generate_204",
    .method = HTTP_GET,
    .handler = get_req_204_handler,
    .user_ctx = NULL};

httpd_uri_t uri_save_config = {
    .uri = "/save-config",
    .method = HTTP_POST,
    .handler = save_config_handler,
    .user_ctx = NULL};

httpd_handle_t setup_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    config.max_open_sockets = 7;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
        httpd_register_uri_handler(server, &uri_204);
        httpd_register_uri_handler(server, &uri_2044);
        httpd_register_uri_handler(server, &uri_save_config);
        httpd_register_uri_handler(server, &uri_get);
        // httpd_register_uri_handler(server, &uri_on);
        // httpd_register_uri_handler(server, &uri_off);
    }

    return server;
}