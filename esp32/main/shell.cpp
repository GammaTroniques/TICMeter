#include "shell.h"
#include "wifi.h"
#include "mqtt.h"
#include "main.h"
#include "gpio.h"
#include "ota.h"

#define TAG "SHELL"

void shellInit()
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();

    repl_config.prompt = "$";
    repl_config.max_cmdline_length = 100;

    esp_console_register_help_command();
    esp_console_register_wifi_command();
    esp_console_register_web_command();
    esp_console_register_mqtt_command();
    esp_console_register_mode_command();
    esp_console_register_config_command();
    esp_console_register_reset_command();
    esp_console_register_VCondo_command();
    esp_console_register_test_led_command();
    esp_console_register_ota_check_command();
    esp_console_register_tuya_command();
    esp_console_register_linky_command();
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

int esp_reset_command(int argc, char **argv)
{
    ESP_LOGI(TAG, "Resetting the device");
    esp_restart();
    return 0;
}
esp_err_t esp_console_register_reset_command(void)
{
    esp_console_cmd_t reset = {
        .command = "reset",
        .help = "Reset the device",
        .func = &esp_reset_command};

    return esp_console_cmd_register(&reset);
}

int get_wifi_command(int argc, char **argv)
{
    if (argc != 1)
    {
        return ESP_ERR_INVALID_ARG;
    }
    printf("SSID: %s\n", config.values.ssid);
    printf("Password: %s\n", config.values.password);

    return 0;
}
int set_wifi_command(int argc, char **argv)
{
    if (argc != 3)
    {
        return ESP_ERR_INVALID_ARG;
    }
    strcpy(config.values.ssid, argv[1]);
    strcpy(config.values.password, argv[2]);
    config.write();
    printf("Wifi credentials saved\n");

    return 0;
}
int connect_wifi_command(int argc, char **argv)
{
    printf("Connecting to wifi\n");
    connectToWifi();
    return 0;
}
int reconnect_wifi_command(int argc, char **argv)
{
    ESP_LOGI(TAG, "%f", getVCondo());
    return 0;
}
int wifi_disconnect_command(int argc, char **argv)
{
    printf("Disconnecting from wifi\n");
    disconectFromWifi();
    return 0;
}
int wifi_status_command(int argc, char **argv)
{
    printf("Wifi status TODO\n");
    // TODO
    return 0;
}
int wifi_start_captive_portal_command(int argc, char **argv)
{
    printf("Starting captive portal TODO\n");
    start_captive_portal();
    return 0;
}
esp_err_t esp_console_register_wifi_command(void)
{
    esp_console_cmd_t get = {
        .command = "get-wifi",
        .help = "Get wifi config",
        .func = &get_wifi_command};
    esp_err_t err = esp_console_cmd_register(&get);
    if (err != ESP_OK)
    {
        return err;
    }

    static struct
    {
        struct arg_str *ssid;
        struct arg_str *password;
        struct arg_end *end;
    } set_wifi_args;

    set_wifi_args.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID of AP");
    set_wifi_args.password = arg_str1(NULL, NULL, "<password>", "Password of AP");
    set_wifi_args.end = arg_end(2);

    esp_console_cmd_t set = {
        .command = "set-wifi",
        .help = "Set wifi config",
        .func = &set_wifi_command,
        .argtable = &set_wifi_args};
    err = esp_console_cmd_register(&set);
    if (err != ESP_OK)
    {
        return err;
    }

    esp_console_cmd_t connect = {
        .command = "wifi-connect",
        .help = "Connect to wifi",
        .func = &connect_wifi_command};
    err = esp_console_cmd_register(&connect);
    if (err != ESP_OK)
    {
        return err;
    }

    esp_console_cmd_t reconnect = {
        .command = "wifi-reconnect",
        .help = "Reconnect to wifi",
        .func = &reconnect_wifi_command};
    err = esp_console_cmd_register(&reconnect);
    if (err != ESP_OK)
    {
        return err;
    }

    esp_console_cmd_t disconnect = {
        .command = "wifi-disconnect",
        .help = "Disconnect from wifi",
        .func = &wifi_disconnect_command};
    err = esp_console_cmd_register(&disconnect);
    if (err != ESP_OK)
    {
        return err;
    }

    esp_console_cmd_t status = {
        .command = "wifi-status",
        .help = "Get wifi status",
        .func = &wifi_status_command};
    err = esp_console_cmd_register(&status);
    if (err != ESP_OK)
    {
        return err;
    }

    esp_console_cmd_t start_captive_portal = {
        .command = "wifi-start-captive-portal",
        .help = "Start captive portal",
        .func = &wifi_start_captive_portal_command};
    err = esp_console_cmd_register(&start_captive_portal);
    if (err != ESP_OK)
    {
        return err;
    }

    return ESP_OK;
}

int set_web_command(int argc, char **argv)
{
    if (argc != 5)
    {
        return ESP_ERR_INVALID_ARG;
    }
    strcpy(config.values.web.host, argv[1]);
    strcpy(config.values.web.postUrl, argv[2]);
    strcpy(config.values.web.configUrl, argv[3]);
    strcpy(config.values.web.token, argv[4]);
    config.write();
    printf("Web credentials saved\n");
    return 0;
}
int get_web_command(int argc, char **argv)
{
    if (argc != 1)
    {
        return ESP_ERR_INVALID_ARG;
    }
    printf("Host: %s\n", config.values.web.host);
    printf("PostUrl: %s\n", config.values.web.postUrl);
    printf("ConfigUrl: %s\n", config.values.web.configUrl);
    printf("Token: %s\n", config.values.web.token);
    return 0;
}
esp_err_t esp_console_register_web_command()
{
    esp_console_cmd_t get = {
        .command = "get-web",
        .help = "Get web config",
        .func = &get_web_command};

    esp_err_t err = esp_console_cmd_register(&get);
    if (err != ESP_OK)
    {
        return err;
    }

    static struct
    {
        struct arg_str *host;
        struct arg_str *postUrl;
        struct arg_str *configUrl;
        struct arg_str *token;
        struct arg_end *end;
    } set_web_args;

    set_web_args.host = arg_str1(NULL, NULL, "<host>", "Host of web server e.g. 192.168.1.10");
    set_web_args.postUrl = arg_str1(NULL, NULL, "<postUrl>", "Url for posting data e.g. /post");
    set_web_args.configUrl = arg_str1(NULL, NULL, "<configUrl>", "Url for getting config e.g. /config");
    set_web_args.token = arg_str1(NULL, NULL, "<token>", "Token for authorization");
    set_web_args.end = arg_end(4);

    esp_console_cmd_t set = {
        .command = "set-web",
        .help = "Set web config",
        .func = &set_web_command,
        .argtable = &set_web_args};
    err = esp_console_cmd_register(&set);
    if (err != ESP_OK)
    {
        return err;
    }

    return ESP_OK;
}

int get_mqtt_command(int argc, char **argv)
{
    if (argc != 1)
    {
        return ESP_ERR_INVALID_ARG;
    }
    printf("Host: %s\n", config.values.mqtt.host);
    printf("Port: %d\n", config.values.mqtt.port);
    printf("Topic: %s\n", config.values.mqtt.topic);
    printf("Username: %s\n", config.values.mqtt.username);
    printf("Password: %s\n", config.values.mqtt.password);
    return 0;
}
int set_mqtt_command(int argc, char **argv)
{
    if (argc != 6)
    {
        return ESP_ERR_INVALID_ARG;
    }
    strcpy(config.values.mqtt.host, argv[1]);
    config.values.mqtt.port = atoi(argv[2]);
    strcpy(config.values.mqtt.topic, argv[3]);
    strcpy(config.values.mqtt.username, argv[4]);
    strcpy(config.values.mqtt.password, argv[5]);
    config.write();
    printf("MQTT credentials saved\n");
    return 0;
}
int mqtt_connect_command(int argc, char **argv)
{
    if (argc != 1)
    {
        return ESP_ERR_INVALID_ARG;
    }
    printf("MQTT connect\n");
    mqtt_app_start();
    return 0;
}
int mqtt_send_command(int argc, char **argv)
{
    if (argc != 1)
    {
        return ESP_ERR_INVALID_ARG;
    }
    printf("MQTT send\n");
    LinkyData data;
    // data.hist->timestamp = getTimestamp();
    // data.hist->BASE = 5050;
    // data.hist->IINST = 10;
    sendToMqtt(&data);
    return 0;
}
int mqtt_discovery_command(int argc, char **argv)
{
    if (argc != 1)
    {
        return ESP_ERR_INVALID_ARG;
    }
    printf("MQTT discovery\n");
    setupHomeAssistantDiscovery();
    return 0;
}
esp_err_t esp_console_register_mqtt_command()
{
    esp_console_cmd_t get = {
        .command = "get-mqtt",
        .help = "Get mqtt config",
        .func = &get_mqtt_command};

    esp_err_t err = esp_console_cmd_register(&get);
    if (err != ESP_OK)
    {
        return err;
    }

    static struct
    {
        struct arg_str *host;
        struct arg_int *port;
        struct arg_str *topic;
        struct arg_str *username;
        struct arg_str *password;
        struct arg_end *end;
    } set_mqtt_args;

    set_mqtt_args.host = arg_str1(NULL, NULL, "<host>", "Host of mqtt server e.g. 192.168.1.10");
    set_mqtt_args.port = arg_int1(NULL, NULL, "<port>", "Port of mqtt server e.g. 1883");
    set_mqtt_args.topic = arg_str1(NULL, NULL, "<topic>", "Topic for publishing data e.g. /topic");
    set_mqtt_args.username = arg_str1(NULL, NULL, "<username>", "Username for authorization");
    set_mqtt_args.password = arg_str1(NULL, NULL, "<password>", "Password for authorization");
    set_mqtt_args.end = arg_end(5);

    esp_console_cmd_t set = {
        .command = "set-mqtt",
        .help = "Set mqtt config",
        .func = &set_mqtt_command,
        .argtable = &set_mqtt_args};
    err = esp_console_cmd_register(&set);
    if (err != ESP_OK)
    {
        return err;
    }
    esp_console_cmd_t connect = {
        .command = "mqtt-connect",
        .help = "Connect to mqtt server",
        .func = &mqtt_connect_command};

    err = esp_console_cmd_register(&connect);
    if (err != ESP_OK)
    {
        return err;
    }

    static struct
    {
        struct arg_str *message;
        struct arg_end *end;
    } send_args;

    send_args.message = arg_str1(NULL, NULL, "<message>", "Message to send");
    send_args.end = arg_end(1);

    esp_console_cmd_t send = {
        .command = "mqtt-send",
        .help = "Send message to mqtt server",
        .func = &mqtt_send_command,
        .argtable = &send_args};
    err = esp_console_cmd_register(&send);
    if (err != ESP_OK)
    {
        return err;
    }

    esp_console_cmd_t discovery = {
        .command = "mqtt-discovery",
        .help = "Send discovery message to mqtt server",
        .func = &mqtt_discovery_command};

    err = esp_console_cmd_register(&discovery);
    if (err != ESP_OK)
    {
        return err;
    }

    return ESP_OK;
}

int get_mode_command(int argc, char **argv)
{
    if (argc != 1)
    {
        return ESP_ERR_INVALID_ARG;
    }
    printf("Mode: %d - %s\n", config.values.mode, MODES[config.values.mode]);
    return 0;
}
int set_mode_command(int argc, char **argv)
{
    if (argc != 2)
    {
        return ESP_ERR_INVALID_ARG;
    }
    config.values.mode = atoi(argv[1]);
    config.write();
    printf("Mode saved\n");
    return 0;
}
esp_err_t esp_console_register_mode_command()
{
    esp_console_cmd_t get = {
        .command = "get-mode",
        .help = "Get mode",
        .func = &get_mode_command};

    esp_err_t err = esp_console_cmd_register(&get);
    if (err != ESP_OK)
    {
        return err;
    }

    static struct
    {
        struct arg_int *mode;
        struct arg_end *end;
    } set_mode_args;

    set_mode_args.mode = arg_int1(NULL, NULL, "<mode>", "Mode of operation");
    set_mode_args.end = arg_end(1);

    esp_console_cmd_t set = {
        .command = "set-mode",
        .help = "Set mode\n"
                "0 - Wifi - Webserver\n"
                "1 - Wifi - MQTT\n"
                "2 - Wifi - MQTT Home Assistant\n"
                "3 - Zigbee\n"
                "4 - Matter\n",

        .func = &set_mode_command,
        .argtable = &set_mode_args};
    err = esp_console_cmd_register(&set);
    if (err != ESP_OK)
    {
        return err;
    }

    return ESP_OK;
}

int get_config_command(int argc, char **argv)
{
    if (argc != 1)
    {
        return ESP_ERR_INVALID_ARG;
    }
    printf("config read\n");
    config.read();
    return 0;
}
int set_config_command(int argc, char **argv)
{
    if (argc != 1)
    {
        return ESP_ERR_INVALID_ARG;
    }
    config.write();
    printf("Config saved\n");
    return 0;
}
esp_err_t esp_console_register_config_command()
{
    esp_console_cmd_t get = {
        .command = "get-config",
        .help = "Get config",
        .func = &get_config_command};

    esp_err_t err = esp_console_cmd_register(&get);
    if (err != ESP_OK)
    {
        return err;
    }

    esp_console_cmd_t set = {
        .command = "set-config",
        .help = "Set config",
        .func = &set_config_command};
    err = esp_console_cmd_register(&set);
    if (err != ESP_OK)
    {
        return err;
    }

    return ESP_OK;
}

int get_VCondo_command(int argc, char **argv)
{
    if (argc != 1)
    {
        return ESP_ERR_INVALID_ARG;
    }
    printf("VCondo: %f\n", getVCondo());
    return 0;
}

esp_err_t esp_console_register_VCondo_command()
{
    esp_console_cmd_t get = {
        .command = "get-VCondo",
        .help = "Get VCondo",
        .func = &get_VCondo_command};

    esp_err_t err = esp_console_cmd_register(&get);
    if (err != ESP_OK)
    {
        return err;
    }

    return ESP_OK;
}

int test_led_command(int argc, char **argv)
{
    if (argc != 2)
    {
        return ESP_ERR_INVALID_ARG;
    }
    startLedPattern(atoi(argv[1]));
    ESP_LOGI(TAG, "Test led pattern %d", atoi(argv[1]));
    return 0;
}

esp_err_t esp_console_register_test_led_command()
{
    static struct
    {
        struct arg_int *mode;
        struct arg_end *end;
    } set_mode_args;

    set_mode_args.mode = arg_int1(NULL, NULL, "<pattern>", "Pattern to test");
    set_mode_args.end = arg_end(1);

    esp_console_cmd_t set = {
        .command = "led-pattern",
        .help = "Test led pattern\n",
        .func = &test_led_command,
        .argtable = &set_mode_args};
    esp_err_t err = esp_console_cmd_register(&set);
    if (err != ESP_OK)
    {
        return err;
    }

    return ESP_OK;
}

int ota_check_command(int argc, char **argv)
{
    if (argc != 1)
    {
        return ESP_ERR_INVALID_ARG;
    }
    check_ota_update();
    return 0;
}
esp_err_t esp_console_register_ota_check_command()
{
    esp_console_cmd_t get = {
        .command = "ota-check",
        .help = "Check for ota update",
        .func = &ota_check_command};

    esp_err_t err = esp_console_cmd_register(&get);
    if (err != ESP_OK)
    {
        return err;
    }

    return ESP_OK;
}

int set_tuya_command(int argc, char **argv)
{
    if (argc != 5)
    {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(config.values.tuya.deviceId, argv[1], 23);
    memcpy(config.values.tuya.deviceSecret, argv[2], 17);
    memcpy(config.values.tuya.productId, argv[3], 16);
    config.values.tuya.server = (tuya_server_t)atoi(argv[4]);
    config.write();
    printf("Tuya config saved\n");
    return 0;
}

int get_tuya_command(int argc, char **argv)
{
    if (argc != 1)
    {
        return ESP_ERR_INVALID_ARG;
    }
    printf("Tuya config:\n");
    printf("Device ID: %s\n", config.values.tuya.deviceId);
    printf("Device Secret: %s\n", config.values.tuya.deviceSecret);
    printf("Product ID: %s\n", config.values.tuya.productId);
    printf("Server: %s\n", TUYA_SERVERS[config.values.tuya.server]);
    return 0;
}

esp_err_t esp_console_register_tuya_command()
{
    static struct
    {
        struct arg_str *deviceId;
        struct arg_str *deviceSecret;
        struct arg_str *productId;
        struct arg_int *server;
        struct arg_end *end;
    } set_mode_args;

    set_mode_args.deviceId = arg_str1(NULL, NULL, "<deviceId>", "Device ID");
    set_mode_args.deviceSecret = arg_str1(NULL, NULL, "<deviceSecret>", "Device Secret");
    set_mode_args.productId = arg_str1(NULL, NULL, "<productId>", "Product ID");
    set_mode_args.server = arg_int1(NULL, NULL, "<server>", "Server (0: CN, 1: EU, 2: US)");
    set_mode_args.end = arg_end(1);

    esp_console_cmd_t set = {
        .command = "set-tuya",
        .help = "Set Tuya config\n",
        .func = &set_tuya_command,
        .argtable = &set_mode_args};
    esp_err_t err = esp_console_cmd_register(&set);
    if (err != ESP_OK)
    {
        return err;
    }

    esp_console_cmd_t get = {
        .command = "get-tuya",
        .help = "Get Tuya config\n",
        .func = &get_tuya_command};
    err = esp_console_cmd_register(&get);
    if (err != ESP_OK)
    {
        return err;
    }

    return ESP_OK;
}

esp_err_t esp_console_register_linky_command()
{
    esp_console_cmd_t get = {
        .command = "get-linky",
        .help = "Get Linky mode",
        .func = &get_linky_mode_command};

    esp_err_t err = esp_console_cmd_register(&get);
    if (err != ESP_OK)
    {
        return err;
    }

    static struct
    {
        struct arg_int *mode;
        struct arg_end *end;
    } set_mode_args;

    set_mode_args.mode = arg_int1(NULL, NULL, "<mode>", "Mode of operation");
    set_mode_args.end = arg_end(1);

    esp_console_cmd_t set = {
        .command = "set-linky",
        .help = "Set Linky mode\n"
                "0: MODE_HISTORIQUE\n"
                "1: MODE_STANDARD\n"
                "2: MODE_AUTO",
        .func = &set_linky_mode_command,
        .argtable = &set_mode_args};
    err = esp_console_cmd_register(&set);
    if (err != ESP_OK)
    {
        return err;
    }

    return ESP_OK;
}

int get_linky_mode_command(int argc, char **argv)
{
    if (argc != 1)
    {
        return ESP_ERR_INVALID_ARG;
    }
    const char *modes[] = {"MODE_HISTORIQUE", "MODE_STANDARD", "MODE_AUTO"};
    printf("Current Linky mode: %d: %s\n", linky.mode, modes[linky.mode]);
    printf("Configured Linky mode: %d: %s\n", config.values.linkyMode, modes[config.values.linkyMode]);
    return 0;
}
int set_linky_mode_command(int argc, char **argv)
{
    if (argc != 2)
    {
        return ESP_ERR_INVALID_ARG;
    }
    config.values.linkyMode = (LinkyMode)atoi(argv[1]);
    config.write();
    printf("Mode saved\n");
    get_linky_mode_command(1, NULL);
    return 0;
}