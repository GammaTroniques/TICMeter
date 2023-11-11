/**
 * @file shell.cpp
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
#include "shell.h"
#include "esp_ota_ops.h"
#include "gpio.h"
#include "main.h"
#include "mqtt.h"
#include "ota.h"
#include "wifi.h"
#include "nvs_flash.h"

/*==============================================================================
 Local Define
===============================================================================*/
#define TAG "SHELL"

#define MAX_ARGS_COUNT 5

/*==============================================================================
 Local Macro
===============================================================================*/

/*==============================================================================
 Local Type
===============================================================================*/
typedef struct
{
  const char *command;
  const char *help;
  int (*func)(int argc, char **argv);
  const uint8_t args_num;
  const char *args[5];
  const char *hint[5];
} shell_cmd_t;

/*==============================================================================
 Local Function Declaration
===============================================================================*/
static int get_wifi_command(int argc, char **argv);
static int set_wifi_command(int argc, char **argv);
static int connect_wifi_command(int argc, char **argv);
static int reconnect_wifi_command(int argc, char **argv);
static int wifi_disconnect_command(int argc, char **argv);
static int wifi_status_command(int argc, char **argv);

static int set_web_command(int argc, char **argv);
static int get_web_command(int argc, char **argv);

static int get_mqtt_command(int argc, char **argv);
static int set_mqtt_command(int argc, char **argv);
static int mqtt_connect_command(int argc, char **argv);
static int mqtt_send_command(int argc, char **argv);

static int get_mode_command(int argc, char **argv);
static int set_mode_command(int argc, char **argv);

static int get_config_command(int argc, char **argv);
static int set_config_command(int argc, char **argv);

static int get_VCondo_command(int argc, char **argv);
static int ota_check_command(int argc, char **argv);

static int set_tuya_command(int argc, char **argv);
static int get_tuya_command(int argc, char **argv);

static int set_linky_mode_command(int argc, char **argv);
static int get_linky_mode_command(int argc, char **argv);
static int linky_print_command(int argc, char **argv);
static int linky_simulate(int argc, char **argv);

static int wifi_start_captive_portal_command(int argc, char **argv);
static int mqtt_discovery_command(int argc, char **argv);
static int test_led_command(int argc, char **argv);

static int get_voltages(int argc, char **argv);

static int set_sleep_command(int argc, char **argv);
static int get_sleep_command(int argc, char **argv);
static int read_nvs(int argc, char **argv);

static int info_command(int argc, char **argv);

static int esp_reset_command(int argc, char **argv);
static int ota_start(int argc, char **argv);

static int set_refresh_command(int argc, char **argv);
static int get_refresh_command(int argc, char **argv);
// static esp_err_t esp_console_register_reset_command(void);
static int led_off(int argc, char **argv);
static int factory_reset(int argc, char **argv);

/*==============================================================================
Public Variable
===============================================================================*/

/*==============================================================================
 Local Variable
===============================================================================*/
// clang-format off
static const shell_cmd_t shell_cmds[] = {
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
    {"set-web",                     "Set web config",                           &set_web_command,                   4, {"<host>", "<postUrl>", "<configUrl>", "<token>"}, {"Host of web server e.g. 192.168.1.10", "Url for posting linky_data e.g. /post", "Url for getting config e.g. /config", "Token for authorization"}},

    //mqtt
    {"get-mqtt",                    "Get mqtt config",                          &get_mqtt_command,                  0, {}, {}},
    {"set-mqtt",                    "Set mqtt config",                          &set_mqtt_command,                  5, {"<host>", "<port>","<topic>", "<username>","<password>"}, {"Host of mqtt server e.g. 192.168.1.10", "Port of mqtt server e.g. 1883", "Topic for publishing linky_data e.g. /topic", "Username for authorization", "Password for authorization"}},
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

    {"set-refresh",                 "Set refresh rate",                         &set_refresh_command,               1, {"<refresh>"}, {"Refresh rate in seconds"}},
    {"get-refresh",                 "Get refresh rate",                         &get_refresh_command,               0, {}, {}},
    {"get-config",                  "Get config",                               &get_config_command,                0, {}, {}},
    {"set-config",                  "Set config",                               &set_config_command,                2, {"<key>", "<value>"}, {"Key", "Value"}},
    {"get-VCondo",                  "Get VCondo",                               &get_VCondo_command,                0, {}, {}},
    {"test-led",                    "Test led",                                 &test_led_command,                  0, {}, {}},
    {"ota-check",                   "Check for OTA update",                     &ota_check_command,                 0, {}, {}},
    {"set-tuya",                    "Set tuya config",                          &set_tuya_command,                  3, {"<product_id>", "<device_uuid>", "<device_auth>"}, {"Product ID", "Device UUID", "Device Auth Key"}},
    {"get-tuya",                    "Get tuya config",                          &get_tuya_command,                  0, {}, {}},
    {"set-linky-mode",              "Set linky mode",                           &set_linky_mode_command,            1, {"<mode>"}, {"Mode"}},
    {"get-linky-mode",              "Get linky mode",                           &get_linky_mode_command,            0, {}, {}},
    {"linky-print",                 "Print linky linky_data",                   &linky_print_command,               0, {}, {}},
    {"linky-simulate",              "Simulate linky linky_data",                &linky_simulate,                    0, {}, {}},
    {"get-voltage",                 "Get Voltages",                             &get_voltages,                      0, {}, {}},
    {"set-sleep",                   "Enable/Disable sleep",                     &set_sleep_command,                 1, {"<enable>"}, {"Enable/Disable deep sleep"}},
    {"get-sleep",                   "Get sleep state",                          &get_sleep_command,                 0, {}, {}},
    {"read-nvs",                    "Read all nvs",                             &read_nvs,                          0, {}, {}},
    {"info",                        "Get system info",                          &info_command,                      0, {}, {}},
    {"ota-start",                   "Start OTA",                                &ota_start,                         0, {}, {}},
    {"led-off",                     "LED OFF",                                  &led_off,                           0, {}, {}},
    {"factory-reset",               "Factory reset",                            &factory_reset,                     0, {}, {}},
};

const uint8_t shell_cmds_num = sizeof(shell_cmds) / sizeof(shell_cmd_t);
// clang-format on
/*==============================================================================
Function Implementation
===============================================================================*/

void shell_init()
{
  esp_log_level_set("wifi", ESP_LOG_ERROR);
  esp_log_level_set("wifi_init", ESP_LOG_ERROR);
  esp_log_level_set("esp_adapter", ESP_LOG_ERROR);
  esp_log_level_set("pp", ESP_LOG_ERROR);
  esp_log_level_set("net80211", ESP_LOG_ERROR);
  esp_log_level_set("esp_netif_handlers", ESP_LOG_ERROR);
  esp_log_level_set("phy_version", ESP_LOG_ERROR);
  esp_log_level_set("phy_init", ESP_LOG_ERROR);
  esp_log_level_set("phy", ESP_LOG_ERROR);
  esp_log_level_set("gpio", ESP_LOG_ERROR);
  esp_log_level_set("uart", ESP_LOG_ERROR);
  esp_log_level_set("NimBLE", ESP_LOG_ERROR);
  // esp_log_level_set("*", ESP_LOG_WARN);

  esp_console_repl_t *repl = NULL;
  esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();

  repl_config.prompt = ">";
  repl_config.max_cmdline_length = 100;

  for (int i = 0; i < shell_cmds_num; i++)
  {
    // ESP_LOGI(TAG, "Registering command %s", shell_cmds[i].command);
    // ESP_LOGI(TAG, "Help: %s", shell_cmds[i].help);
    // ESP_LOGI(TAG, "Func: %p", shell_cmds[i].func);
    // ESP_LOGI(TAG, "Args num: %d", shell_cmds[i].args_num);
    esp_console_cmd_t cmd = {.command = shell_cmds[i].command,
                             .help = shell_cmds[i].help,
                             .func = shell_cmds[i].func};

    struct arg_str *args[MAX_ARGS_COUNT + 1] = {NULL};

    if (shell_cmds[i].args_num > 0)
    {
      for (int j = 0; j < shell_cmds[i].args_num; j++)
      {
        args[j] =
            arg_str1(NULL, NULL, shell_cmds[i].args[j], shell_cmds[i].hint[j]);
        ESP_LOGD(TAG, "arg[%d]: %p", j, args[j]);
      }
      struct arg_end *end = arg_end(MAX_ARGS_COUNT);
      args[shell_cmds[i].args_num] = (struct arg_str *)end;
      ESP_LOGD(TAG, "arg[%d]: %p", shell_cmds[i].args_num, args[shell_cmds[i].args_num]);
      cmd.argtable = args;
    }
    esp_console_cmd_register(&cmd);
  }

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || \
    defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
  esp_console_dev_uart_config_t hw_config =
      ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));
#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
  esp_console_dev_usb_serial_jtag_config_t hw_config =
      ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(
      esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));
#endif
  ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

static int esp_reset_command(int argc, char **argv)
{
  ESP_LOGI(TAG, "Resetting the device");
  esp_restart();
  return 0;
}

static int get_wifi_command(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  printf("SSID: %s\n", config_values.ssid);
  printf("Password: %s\n", config_values.password);

  return 0;
}
static int set_wifi_command(int argc, char **argv)
{
  if (argc != 3)
  {
    return ESP_ERR_INVALID_ARG;
  }
  strncpy(config_values.ssid, argv[1], sizeof(config_values.ssid));
  strncpy(config_values.password, argv[2], sizeof(config_values.password));
  config_write();
  printf("Wifi credentials saved\n");

  return 0;
}
static int connect_wifi_command(int argc, char **argv)
{
  printf("Connecting to wifi\n");
  wifi_connect();
  return 0;
}
static int reconnect_wifi_command(int argc, char **argv)
{
  ESP_LOGI(TAG, "%f", gpio_get_vcondo());
  return 0;
}
static int wifi_disconnect_command(int argc, char **argv)
{
  printf("Disconnecting from wifi\n");
  wifi_disconnect();
  return 0;
}
static int wifi_status_command(int argc, char **argv)
{
  printf("Wifi status TODO\n");
  // TODO
  return 0;
}
static int wifi_start_captive_portal_command(int argc, char **argv)
{
  printf("Starting captive portal TODO\n");
  wifi_start_captive_portal();
  return 0;
}
static int set_web_command(int argc, char **argv)
{
  if (argc != 5)
  {
    return ESP_ERR_INVALID_ARG;
  }
  strncpy(config_values.web.host, argv[1], sizeof(config_values.web.host));
  strncpy(config_values.web.postUrl, argv[2],
          sizeof(config_values.web.postUrl));
  strncpy(config_values.web.configUrl, argv[3],
          sizeof(config_values.web.configUrl));
  strncpy(config_values.web.token, argv[4], sizeof(config_values.web.token));
  config_write();
  printf("Web credentials saved\n");
  return 0;
}
static int get_web_command(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  printf("Host: %s\n", config_values.web.host);
  printf("PostUrl: %s\n", config_values.web.postUrl);
  printf("ConfigUrl: %s\n", config_values.web.configUrl);
  printf("Token: %s\n", config_values.web.token);
  return 0;
}

static int get_mqtt_command(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  printf("Host: %s\n", config_values.mqtt.host);
  printf("Port: %d\n", config_values.mqtt.port);
  printf("Topic: %s\n", config_values.mqtt.topic);
  printf("Username: %s\n", config_values.mqtt.username);
  printf("Password: %s\n", config_values.mqtt.password);
  return 0;
}
static int set_mqtt_command(int argc, char **argv)
{
  if (argc != 6)
  {
    return ESP_ERR_INVALID_ARG;
  }
  strncpy(config_values.mqtt.host, argv[1], sizeof(config_values.mqtt.host));
  config_values.mqtt.port = atoi(argv[2]);
  strncpy(config_values.mqtt.topic, argv[3], sizeof(config_values.mqtt.topic));
  strncpy(config_values.mqtt.username, argv[4],
          sizeof(config_values.mqtt.username));
  strncpy(config_values.mqtt.password, argv[5],
          sizeof(config_values.mqtt.password));
  config_write();
  printf("MQTT credentials saved\n");
  return 0;
}
static int mqtt_connect_command(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  printf("MQTT connect\n");
  mqtt_init();
  return 0;
}
static int mqtt_send_command(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  printf("MQTT send\n");
  LinkyData linky_data;
  // linky_data.hist->timestamp = wifi_get_timestamp();
  // linky_data.hist->BASE = 5050;
  // linky_data.hist->IINST = 10;
  mqtt_send(&linky_data);
  return 0;
}
static int mqtt_discovery_command(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  printf("MQTT discovery\n");
  mqtt_setup_ha_discovery();
  return 0;
}

static int get_mode_command(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  printf("Mode: %d - %s\n", config_values.mode, MODES[config_values.mode]);
  return 0;
}
static int set_mode_command(int argc, char **argv)
{
  if (argc != 2)
  {
    return ESP_ERR_INVALID_ARG;
  }
  config_values.mode = (connectivity_t)atoi(argv[1]);
  config_write();
  printf("Mode saved\n");
  get_mode_command(1, NULL);
  return 0;
}

static int get_config_command(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  printf("config read\n");
  config_read();
  return 0;
}
static int set_config_command(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  config_write();
  printf("Config saved\n");
  return 0;
}

static int get_VCondo_command(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  printf("VCondo: %f\n", gpio_get_vcondo());
  return 0;
}

static int test_led_command(int argc, char **argv)
{
  if (argc != 2)
  {
    return ESP_ERR_INVALID_ARG;
  }
  gpio_start_led_pattern(atoi(argv[1]));
  ESP_LOGI(TAG, "Test led pattern %d", atoi(argv[1]));
  return 0;
}

static int ota_check_command(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  ota_version_t version = {0};
  ota_get_latest(&version);
  return 0;
}

static int set_tuya_command(int argc, char **argv)
{
  if (argc != 4)
  {
    return ESP_ERR_INVALID_ARG;
  }
  memcpy(config_values.tuya.product_id, argv[1],
         sizeof(config_values.tuya.product_id));
  memcpy(config_values.tuya.device_uuid, argv[2],
         sizeof(config_values.tuya.device_uuid));
  memcpy(config_values.tuya.device_auth, argv[3],
         sizeof(config_values.tuya.device_auth));
  config_write();
  printf("Tuya config saved\n");
  get_tuya_command(1, NULL);
  return 0;
}

static int get_tuya_command(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  printf("%cTuya config:\n", 0x02);
  printf("Product ID: %s\n", config_values.tuya.product_id);
  printf("Device UUID: %s\n", config_values.tuya.device_uuid);
  printf("Device Auth: %s\n", config_values.tuya.device_auth);
  printf("Tuya Bind Status: %d%c\n", config_values.tuya.pairing_state, 0x03);
  return 0;
}

static int get_linky_mode_command(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  const char *modes[] = {"MODE_HISTORIQUE", "MODE_STANDARD", "MODE_AUTO"};
  printf("Current Linky mode: %d: %s\n", linky_mode, modes[linky_mode]);
  printf("Configured Linky mode: %d: %s\n", config_values.linkyMode,
         modes[config_values.linkyMode]);
  return 0;
}
static int set_linky_mode_command(int argc, char **argv)
{
  if (argc != 2)
  {
    return ESP_ERR_INVALID_ARG;
  }
  config_values.linkyMode = (linky_mode_t)atoi(argv[1]);
  config_write();
  printf("Mode saved\n");
  get_linky_mode_command(1, NULL);
  return 0;
}

static int linky_print_command(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  linky_print();
  return 0;
}

static int linky_simulate(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  linky_want_debug_frame = true;
  return 0;
}

static int get_voltages(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  printf("VCondo: %f\n", gpio_get_vcondo());
  printf("VUSB: %f\n", gpio_get_vusb());
  return 0;
}

static int set_sleep_command(int argc, char **argv)
{
  if (argc != 2)
  {
    return ESP_ERR_INVALID_ARG;
  }
  config_values.sleep = atoi(argv[1]);
  config_write();
  printf("Sleep saved\n");
  get_sleep_command(1, NULL);
  return 0;
}

static int get_sleep_command(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  printf("Sleep: %d\n", config_values.sleep);
  return 0;
}

static int read_nvs(int argc, char **argv)
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

static int info_command(int argc, char **argv)
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

static int ota_start(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }

  suspendTask(fetchLinkyDataTaskHandle);
  const esp_partition_t *configured = esp_ota_get_boot_partition();
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

  if (configured)
  {
    ESP_LOGI(TAG, "Configured partition : %s, size: %ld", configured->label, configured->size);
  }
  if (running)
  {
    ESP_LOGI(TAG, "Running partition : %s, size: %ld", running->label, running->size);
  }
  if (update_partition)
  {
    ESP_LOGI(TAG, "Update partition : %s, size: %ld", update_partition->label, update_partition->size);
  }
  xTaskCreate(&ota_perform_task, "ota_perform_task", 8192, NULL, 1, NULL);
  return 0;
}

static int set_refresh_command(int argc, char **argv)
{
  if (argc != 2)
  {
    return ESP_ERR_INVALID_ARG;
  }
  config_values.refreshRate = atoi(argv[1]);
  config_write();
  printf("Refresh saved\n");
  get_refresh_command(1, NULL);
  return 0;
}

static int get_refresh_command(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  printf("Refresh: %d\n", config_values.refreshRate);
  return 0;
}

static int led_off(int argc, char **argv)
{
  gpio_set_level(LED_EN, 0);
  gpio_set_direction(LED_DATA, GPIO_MODE_INPUT); // HIGH-Z
  return 0;
}

static int factory_reset(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  config_erase();
  printf("Factory reset done\n");
  config_write();
  return 0;
}
