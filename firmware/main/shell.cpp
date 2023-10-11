#include "shell.h"
#include "wifi.h"
#include "mqtt.h"
#include "main.h"
#include "gpio.h"
#include "esp_ota_ops.h"
#include "ota.h"

#define TAG "SHELL"

struct shell_cmd_t
{
    const char *command;
    const char *help;
    int (*func)(int argc, char **argv);
    const uint8_t args_num;
    const char *args[5] = {0};
    const char *hint[5] = {0};
};
// clang-format off

const struct shell_cmd_t shell_cmds[]
{
    // commands                       Help                                        Function                            Args num, Args, Hint
    {"reset",                       "Reset the device",                         &esp_reset_command,                 0, {}, {}},
    {"get-wifi",                    "Get wifi config",                          &get_wifi_command,                  0, {}, {}},
    {"set-wifi",                    "Set wifi config",                          &set_wifi_command,                  2, {"<ssid>", "<password>"}, {"SSID of AP", "Password of AP"}},
    {"wifi-connect",                "Connect to wifi",                          &connect_wifi_command,              0, {}, {}},
    {"wifi-reconnect",              "Reconnect to wifi",                        &reconnect_wifi_command,            0, {}, {}},
    {"wifi-disconnect",             "Disconnect from wifi",                     &wifi_disconnect_command,           0, {}, {}},
    {"wifi-status",                 "Get wifi status",                          &wifi_status_command,               0, {}, {}},
    {"wifi-start-captive-portal",   "Start captive portal",                     &wifi_start_captive_portal_command, 0, {}, {}},
    {"get-web",                     "Get web config",                           &get_web_command,                   0, {}, {}},
    {"set-web",                     "Set web config",                           &set_web_command,                   4, {"<host>", "<postUrl>", "<configUrl>", "<token>"}, {"Host of web server e.g. 192.168.1.10", "Url for posting data e.g. /post", "Url for getting config e.g. /config", "Token for authorization"}},

    //mqtt
    {"get-mqtt",                    "Get mqtt config",                          &get_mqtt_command,                  0, {}, {}},
    {"set-mqtt",                    "Set mqtt config",                          &set_mqtt_command,                  5, {"<host>", "<port>","<topic>", "<username>","<password>"}, {"Host of mqtt server e.g. 192.168.1.10", "Port of mqtt server e.g. 1883", "Topic for publishing data e.g. /topic", "Username for authorization", "Password for authorization"}},
    {"mqtt-connect",                "Connect to mqtt server",                   &mqtt_connect_command,              0, {}, {}},
    {"mqtt-send",                   "Send message to mqtt server",              &mqtt_send_command,                 0, {}, {}},
    {"mqtt-discovery",              "Send discovery message to mqtt server",    &mqtt_discovery_command,            0, {}, {}},

    //mode
    {"get-mode",                    "Get mode",                                 &get_mode_command,                  0, {}, {}},
    {"set-mode",                    "Set mode\n"
                                    "0 - Wifi - Webserver\n"
                                    "1 - Wifi - MQTT\n"
                                    "2 - Wifi - MQTT Home Assistant\n"
                                    "3 - Zigbee\n"
                                    "4 - Matter\n",                             &set_mode_command,                  1, {"<mode>"}, {"Mode of operation"}},

    {"get-config",                  "Get config",                               &get_config_command,                0, {}, {}},
    {"set-config",                  "Set config",                               &set_config_command,                2, {"<key>", "<value>"}, {"Key", "Value"}},
    {"get-VCondo",                  "Get VCondo",                               &get_VCondo_command,                0, {}, {}},
    {"test-led",                    "Test led",                                 &test_led_command,                  0, {}, {}},
    {"ota-check",                   "Check for OTA update",                     &ota_check_command,                 0, {}, {}},
    {"set-tuya",                    "Set tuya config",                          &set_tuya_command,                  3, {"<productID>", "<deviceUUID>", "<deviceAuth>"}, {"Product ID", "Device UUID", "Device Auth Key"}},
    {"get-tuya",                    "Get tuya config",                          &get_tuya_command,                  0, {}, {}},
    {"set-linky-mode",              "Set linky mode",                           &set_linky_mode_command,            1, {"<mode>"}, {"Mode"}},
    {"get-linky-mode",              "Get linky mode",                           &get_linky_mode_command,            0, {}, {}},
    {"linky-print",                 "Print linky data",                         &linky_print_command,               0, {}, {}},
    {"get-voltage",                 "Get Voltages",                             &get_voltages,                      0, {}, {}},
    {"set-sleep",                   "Enable/Disable sleep",                     &set_sleep_command,                 1, {"<enable>"}, {"Enable/Disable deep sleep"}},
    {"get-sleep",                   "Get sleep state",                          &get_sleep_command,                 0, {}, {}},
    {"read-nvs",                    "Read all nvs",                             &read_nvs,                          0, {}, {}},
    {"info",                        "Get system info",                          &info_command,                      0, {}, {}},


};
// clang-format on

void shellInit()
{
    esp_log_level_set("wifi", ESP_LOG_ERROR);
    esp_log_level_set("wifi_init", ESP_LOG_ERROR);
    esp_log_level_set("esp_adapter", ESP_LOG_ERROR);
    esp_log_level_set("pp", ESP_LOG_ERROR);
    esp_log_level_set("net80211", ESP_LOG_ERROR);
    esp_log_level_set("esp_netif_handlers", ESP_LOG_ERROR);
    esp_log_level_set("phy_version", ESP_LOG_ERROR);
    esp_log_level_set("phy_init", ESP_LOG_ERROR);
    esp_log_level_set("gpio", ESP_LOG_ERROR);
    esp_log_level_set("uart", ESP_LOG_ERROR);
    // esp_log_level_set("*", ESP_LOG_WARN);

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();

    repl_config.prompt = "$";
    repl_config.max_cmdline_length = 100;

    for (int i = 0; i < sizeof(shell_cmds) / sizeof(shell_cmd_t); i++)
    {
        // ESP_LOGI(TAG, "Registering command %s", shell_cmds[i].command);
        // ESP_LOGI(TAG, "Help: %s", shell_cmds[i].help);
        // ESP_LOGI(TAG, "Func: %p", shell_cmds[i].func);
        // ESP_LOGI(TAG, "Args num: %d", shell_cmds[i].args_num);
        esp_console_cmd_t cmd = {
            .command = shell_cmds[i].command,
            .help = shell_cmds[i].help,
            .func = shell_cmds[i].func};

        struct arg_str *args[shell_cmds[i].args_num + 1] = {0};

        if (shell_cmds[i].args_num > 0)
        {
            for (int j = 0; j < shell_cmds[i].args_num; j++)
            {
                args[j] = arg_str1(NULL, NULL, shell_cmds[i].args[j], shell_cmds[i].hint[j]);
            }
            struct arg_end *end = arg_end(shell_cmds[i].args_num);
            args[shell_cmds[i].args_num] = (arg_str *)end;
            cmd.argtable = &args;
        }
        esp_console_cmd_register(&cmd);
    }
#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));
#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));
#endif
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

int esp_reset_command(int argc, char **argv)
{
    ESP_LOGI(TAG, "Resetting the device");
    esp_restart();
    return 0;
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
    ESP_LOGI(TAG, "%f", gpio_get_vcondo());
    return 0;
}
int wifi_disconnect_command(int argc, char **argv)
{
    printf("Disconnecting from wifi\n");
    disconnectFromWifi();
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
    mqtt_send(&data);
    return 0;
}
int mqtt_discovery_command(int argc, char **argv)
{
    if (argc != 1)
    {
        return ESP_ERR_INVALID_ARG;
    }
    printf("MQTT discovery\n");
    mqtt_setup_ha_discovery();
    return 0;
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
    config.values.mode = (connectivity_t)atoi(argv[1]);
    config.write();
    printf("Mode saved\n");
    get_mode_command(1, NULL);
    return 0;
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

int get_VCondo_command(int argc, char **argv)
{
    if (argc != 1)
    {
        return ESP_ERR_INVALID_ARG;
    }
    printf("VCondo: %f\n", gpio_get_vcondo());
    return 0;
}

int test_led_command(int argc, char **argv)
{
    if (argc != 2)
    {
        return ESP_ERR_INVALID_ARG;
    }
    gpio_start_led_pattern(atoi(argv[1]));
    ESP_LOGI(TAG, "Test led pattern %d", atoi(argv[1]));
    return 0;
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

int set_tuya_command(int argc, char **argv)
{
    if (argc != 4)
    {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(config.values.tuyaKeys.productID, argv[1], sizeof(config.values.tuyaKeys.productID));
    memcpy(config.values.tuyaKeys.deviceUUID, argv[2], sizeof(config.values.tuyaKeys.deviceUUID));
    memcpy(config.values.tuyaKeys.deviceAuth, argv[3], sizeof(config.values.tuyaKeys.deviceAuth));
    config.write();
    printf("Tuya config saved\n");
    get_tuya_command(1, NULL);
    return 0;
}

int get_tuya_command(int argc, char **argv)
{
    if (argc != 1)
    {
        return ESP_ERR_INVALID_ARG;
    }
    printf("%cTuya config:\n", 0x02);
    printf("Product ID: %s\n", config.values.tuyaKeys.productID);
    printf("Device UUID: %s\n", config.values.tuyaKeys.deviceUUID);
    printf("Device Auth: %s\n", config.values.tuyaKeys.deviceAuth);
    printf("Tuya Bind Status: %d%c\n", config.values.tuyaBinded, 0x03);
    return 0;
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

int linky_print_command(int argc, char **argv)
{
    if (argc != 1)
    {
        return ESP_ERR_INVALID_ARG;
    }
    linky.print();
    return 0;
}

int get_voltages(int argc, char **argv)
{
    if (argc != 1)
    {
        return ESP_ERR_INVALID_ARG;
    }
    printf("VCondo: %f\n", gpio_get_vcondo());
    printf("VUSB: %f\n", gpio_get_vusb());
    return 0;
}

int set_sleep_command(int argc, char **argv)
{
    if (argc != 2)
    {
        return ESP_ERR_INVALID_ARG;
    }
    config.values.sleep = atoi(argv[1]);
    config.write();
    printf("Sleep saved\n");
    get_sleep_command(1, NULL);
    return 0;
}

int get_sleep_command(int argc, char **argv)
{
    if (argc != 1)
    {
        return ESP_ERR_INVALID_ARG;
    }
    printf("Sleep: %d\n", config.values.sleep);
    return 0;
}

int read_nvs(int argc, char **argv)
{
    if (argc != 1)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // nvs_iterator_t it = nvs_entry_find(part, NULL, NVS_TYPE_ANY);
    // nvs_entry_info_t info;
    // while (it)
    // {
    //     nvs_entry_info(it, &info);
    //     printf("%s::%s type=%d\n", info.namespace_name, info.key, info.type);
    //     it = nvs_entry_next(it);
    // }
    return 0;
}

int info_command(int argc, char **argv)
{
    if (argc != 1)
    {
        return ESP_ERR_INVALID_ARG;
    }
    const esp_app_desc_t *app_desc = esp_app_get_description();
    printf("%cApp version: %s\n", 0x02, app_desc->version);
    printf("Git commit: %s\n", GIT_REV);
    printf("Git tag: %s\n", GIT_TAG);
    printf("Git branch: %s\n", GIT_BRANCH);
    printf("Build time: %s\n", BUILD_TIME);
    printf("%c", 3);
    return 0;
}