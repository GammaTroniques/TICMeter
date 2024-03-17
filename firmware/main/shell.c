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
#include "esp_timer.h"
#include "gpio.h"
#include "main.h"
#include "mqtt.h"
#include "ota.h"
#include "wifi.h"
#include "tests.h"
#include "nvs_flash.h"
#include "esp_private/periph_ctrl.h"
#include "soc/periph_defs.h"
#include "esp_pm.h"
#include "led.h"
#include "tuya.h"
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
static int set_tuya_pairing(int argc, char **argv);
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

static int rw_command(int argc, char **argv);
static int efuse_read(int argc, char **argv);
static int efuse_write(int argc, char **argv);
static int nvs_stats(int argc, char **argv);
static int print_task_list(int argc, char **argv);
static int start_test_command(int argc, char **argv);

static int zigbee_reset_command(int argc, char **argv);
static int skip_command(int argc, char **argv);

static int start_pairing_command(int argc, char **argv);
static int pm_stats_command(int argc, char **argv);
static int wifi_scan_command(int argc, char **argv);

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
    {"set-config",                  "Set config",                               &set_config_command,                0, {}, {}},
    {"get-VCondo",                  "Get VCondo",                               &get_VCondo_command,                0, {}, {}},
    {"test-led",                    "Test led",                                 &test_led_command,                  0, {}, {}},
    {"ota-check",                   "Check for OTA update",                     &ota_check_command,                 0, {}, {}},
    {"set-tuya",                    "Set config",                               &set_tuya_command,                  2, {"<device_uuid>", "<device_auth>"}, {"Device UUID", "Device Auth Key"}},
    {"set-tuya-pairing",            "Set tuya pairing state",                   &set_tuya_pairing,                  1, {"<pairing_state>"}, {"Pairing state"}},
    {"get-tuya",                    "Get tuya config",                          &get_tuya_command,                  0, {}, {}},
    {"set-linky-mode",              "Set linky mode",                           &set_linky_mode_command,            1, {"<mode>"}, {"Mode"}},
    {"get-linky-mode",              "Get linky mode",                           &get_linky_mode_command,            0, {}, {}},
    {"linky-print",                 "Print linky linky_data",                   &linky_print_command,               1, {"<debug>"}, {"View raw frame, bool 0/1"}},
    {"linky-simulate",              "Simulate linky linky_data",                &linky_simulate,                    1, {"<std>"}, {"Mode STD ? 0/1"}},
    {"get-voltage",                 "Get Voltages",                             &get_voltages,                      0, {}, {}},
    {"set-sleep",                   "Enable/Disable sleep",                     &set_sleep_command,                 1, {"<enable>"}, {"Enable/Disable deep sleep"}},
    {"get-sleep",                   "Get sleep state",                          &get_sleep_command,                 0, {}, {}},
    {"read-nvs",                    "Read all nvs",                             &read_nvs,                          0, {}, {}},
    {"info",                        "Get system info",                          &info_command,                      0, {}, {}},
    {"ota-start",                   "Start OTA",                                &ota_start,                         0, {}, {}},
    {"led-off",                     "LED OFF",                                  &led_off,                           0, {}, {}},
    {"factory-reset",               "Factory reset",                            &factory_reset,                     1, {"<nvs_erase>"}, {"Full nvs clear (0/1)"}},
    {"rw",                          "Open RO partition in RW mode",             &rw_command,                        0, {}, {}},
    {"efuse-read",                  "Read efuse",                               &efuse_read,                        0, {}, {}},
    {"efuse-write",                 "Write efuse",                              &efuse_write,                       1, {"<serialnumber>"}, {"The serial number to write"}},
    {"nvs-stats",                   "Print nvs stats",                          &nvs_stats,                         0, {}, {}},
    {"task-list",                   "Print task list",                          &print_task_list,                   0, {}, {}},
    {"start-test",                  "Start a test",                             &start_test_command,                1, {"<test-name>"}, {"Available tests: adc"}},
    {"zigbee-reset",                "Clear Zigbee config",                      &zigbee_reset_command,              0, {}, {}},
    {"skip",                        "Skip refresh rate delay",                  &skip_command,                      0, {}, {}},
    {"pairing",                     "Start pairing",                            &start_pairing_command,             0, {}, {}},
    {"pm-stats",                    "Power management stats",                   &pm_stats_command,                  0, {}, {}},
    {"wifi-scan",                   "Scan for wifi networks",                   &wifi_scan_command,                 0, {}, {}},

};
const uint8_t shell_cmds_num = sizeof(shell_cmds) / sizeof(shell_cmd_t);
// clang-format on
/*==============================================================================
Function Implementation
===============================================================================*/
void null_printf(const char *fmt, ...)
{
}
esp_console_repl_t *shell_repl = NULL;

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
  esp_log_level_set("adc_hal", ESP_LOG_ERROR);
  // esp_log_level_set("*", ESP_LOG_WARN);

  esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();

  repl_config.prompt = ">";
  repl_config.max_cmdline_length = 100;
  // repl_config.task_priority = PRIORITY_SHELL;

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
  ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &shell_repl));
#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
  esp_console_dev_usb_serial_jtag_config_t hw_config =
      ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(
      esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &shell_repl));
#endif

  // FILE *before = freopen("/dev/null", "w", stdout);
  // int fd = dup(fileno(stdout));
  ESP_ERROR_CHECK(esp_console_start_repl(shell_repl));
  // vTaskDelay(1000 / portTICK_PERIOD_MS);
  // dup2(fd, fileno(stdout));
  // close(fd);
}

void shell_deinit()
{
  ESP_LOGI(TAG, "Deinit shell");
  shell_repl->del(shell_repl);
}

void shell_reinit()
{
  ESP_LOGI(TAG, "Reinit shell");
  esp_console_start_repl(shell_repl);
}

static int esp_reset_command(int argc, char **argv)
{
  ESP_LOGI(TAG, "Resetting the device");
  hard_restart();
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
  esp_err_t err = wifi_connect();
  ESP_LOGI(TAG, "Wifi connect: %d", err);
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
  linky_data_t linky_data;
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
  led_start_pattern(atoi(argv[1]));
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
  if (argc != 3)
  {
    return ESP_ERR_INVALID_ARG;
  }
  memcpy(config_values.tuya.device_uuid, argv[1],
         sizeof(config_values.tuya.device_uuid));
  memcpy(config_values.tuya.device_auth, argv[2],
         sizeof(config_values.tuya.device_auth));
  config_write();
  printf("Tuya config saved\n");
  get_tuya_command(1, NULL);
  return 0;
}

static int set_tuya_pairing(int argc, char **argv)
{
  if (argc != 2)
  {
    return ESP_ERR_INVALID_ARG;
  }
  config_values.pairing_state = (pairing_state_t)atoi(argv[1]);
  config_write();
  printf("Tuya pairing state saved\n");
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
  printf("Product ID: %s\n", TUYA_PRODUCT_ID);
  printf("Device UUID: %s\n", config_values.tuya.device_uuid);
  printf("Device Auth: %s\n", config_values.tuya.device_auth);
  printf("Tuya Bind Status: %d%c\n", config_values.pairing_state, 0x03);
  return 0;
}

static int get_linky_mode_command(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }

  printf("Current Linky mode: %d: %s\n", linky_mode, linky_str_mode[linky_mode]);
  printf("Last Known Linky mode: %d: %s\n", config_values.last_linky_mode, linky_str_mode[config_values.last_linky_mode]);
  printf("Configured Linky mode: %d: %s\n", config_values.linkyMode, linky_str_mode[config_values.linkyMode]);
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
  printf("Mode saved: %d\n", config_values.linkyMode);
  get_linky_mode_command(1, NULL);
  return 0;
}

static int linky_print_command(int argc, char **argv)
{
  if (argc < 1 || argc > 2)
  {
    return ESP_ERR_INVALID_ARG;
  }

  if (argc == 2)
  {
    if (atoi(argv[1]) == 1)
    {
      linky_print_debug_frame();
      return 0;
    }
  }
  linky_print();
  return 0;
}

static int linky_simulate(int argc, char **argv)
{
  if (argc > 2)
  {
    return ESP_ERR_INVALID_ARG;
  }

  if (argc == 2 && atoi(argv[1]) == 1)
  {
    linky_want_debug_frame = 2;
  }
  else
  {
    linky_want_debug_frame = 1;
  }

  return 0;
}

static int get_voltages(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  printf("VCondo: %f\n", gpio_get_vcondo());
  printf("VUSB: %d\n", gpio_vusb_connected());
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
  printf("Up time: %lld s\n", esp_timer_get_time() / 1000000);
  printf("%c", 3);
  return 0;
}

static int ota_start(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }

  suspendTask(main_task_handle);
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
  xTaskCreate(&ota_perform_task, "ota_perform_task", 8192, NULL, PRIORITY_OTA, NULL);
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
  config_factory_reset();
  return 0;
}

static int rw_command(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  config_rw();
  return 0;
}

static int efuse_read(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  config_efuse_read();
  printf("\x02Serial number: %s\n\x03", efuse_values.serialNumber);

  return 0;
}

static int efuse_write(int argc, char **argv)
{
  if (argc != 2)
  {
    return ESP_ERR_INVALID_ARG;
  }

  printf("\x02");
  if (strlen(argv[1]) > sizeof(efuse_values.serialNumber) - 1)
  {
    ESP_LOGE(TAG, "Serial number too long");
    return ESP_ERR_INVALID_ARG;
  }

  if (strlen(efuse_values.serialNumber) > 0)
  {
    printf("Error: cannot write serial number to efuse, already written\n");
    return 0;
  }

  printf("Are you sure you want to write the serial number \"%s\" to the efuse?\n", argv[1]);
  printf("This action is irreversible!\n");
  printf("Type 'YES' to confirm\n");
  char input[4];
  if (fgets(input, sizeof(input), stdin) == NULL)
  {
    // Gestion d'une erreur de lecture
    printf("Error reading input\n");
    return ESP_ERR_INVALID_ARG;
  }
  input[3] = '\0';
  if (strcmp(input, "YES") != 0)
  {
    printf("Aborting\n");
    return 0;
  }

  if (config_efuse_write(argv[1], strlen(argv[1])) == 0)
  {
    printf("Serial number written to efuse\n");
  }
  printf("\x03");

  return 0;
}

static int nvs_stats(int argc, char **argv)
{
  nvs_stats_t stats;
  ESP_ERROR_CHECK(nvs_get_stats(NULL, &stats));
  printf(
      "Used entries: %3zu\t"
      "Free entries: %3zu\t"
      "Total entries: %3zu\t"
      "Namespace count: %3zu\n",
      stats.used_entries,
      stats.free_entries,
      stats.total_entries,
      stats.namespace_count);

  nvs_iterator_t it = NULL;
  esp_err_t res = nvs_entry_find("nvs", NULL, NVS_TYPE_ANY, &it);
  ESP_LOGI(TAG, "nvs_entry_find: 0x%x", res);
  while (res == ESP_OK)
  {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info); // Can omit error check if parameters are guaranteed to be non-NULL
    printf("Namespace: %s\tKey: %s\tType: %d\t\n", info.namespace_name, info.key, info.type);
    res = nvs_entry_next(&it);
  }
  nvs_release_iterator(it);
  return 0;
}

static int print_task_list(int argc, char **argv)
{
  printf("NOT IMPLEMENTED\n");
  // printf("Task Name\tStatus\tPrio\tHWM\tTask\tAffinity\n");
  // char stats_buffer[1024];
  // vTaskList(stats_buffer);
  // printf("%s\n", stats_buffer);
  return 0;
}

static int start_test_command(int argc, char **argv)
{
  if (argc == 1)
  {
    printf("Available tests: ");
    for (int i = 0; i < tests_count; i++)
    {
      printf("%s ", tests_str_available_tests[i]);
    }
    printf("\n");
    return 0;
  }
  if (argc != 2)
  {
    return ESP_ERR_INVALID_ARG;
  }

  char *test_name = argv[1];
  for (int i = 0; i < tests_count; i++)
  {
    if (strcmp(test_name, tests_str_available_tests[i]) == 0)
    {
      start_test((tests_t)i);
      return 0;
    }
  }
  printf("Test not found\n");
  return 0;
}

static int zigbee_reset_command(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  printf("Resetting Zigbee config\n");
  esp_zb_factory_reset();
  printf("Zigbee config reset\n");
  return 0;
}

static int skip_command(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  main_sleep_time = 1;
  return 0;
}

static int start_pairing_command(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  printf("Starting pairing\n");
  gpio_start_pariring();
  return 0;
}

void shell_wake_reason()
{
  switch (esp_reset_reason())
  {
  case ESP_RST_UNKNOWN:
    ESP_LOGI(TAG, "Reset reason: unknown");
    break;
  case ESP_RST_POWERON:
    ESP_LOGI(TAG, "Reset reason: power on");
    break;
  case ESP_RST_EXT:
    ESP_LOGI(TAG, "Reset reason: external");
    break;
  case ESP_RST_SW:
    ESP_LOGI(TAG, "Reset reason: software");
    break;
  case ESP_RST_PANIC:
    ESP_LOGE(TAG, "Reset reason: panic");
    break;
  case ESP_RST_INT_WDT:
    ESP_LOGE(TAG, "Reset reason: interrupt watchdog");
    break;
  case ESP_RST_TASK_WDT:
    ESP_LOGE(TAG, "Reset reason: task watchdog");
    break;
  case ESP_RST_WDT:
    ESP_LOGE(TAG, "Reset reason: watchdog");
    break;
  case ESP_RST_DEEPSLEEP:
    ESP_LOGI(TAG, "Reset reason: deep sleep");
    break;
  case ESP_RST_BROWNOUT:
    ESP_LOGE(TAG, "Reset reason: brownout");

    break;
  case ESP_RST_SDIO:
    ESP_LOGI(TAG, "Reset reason: SDIO");
    break;
  default:
    break;
  }
}

static int pm_stats_command(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  esp_pm_dump_locks(stdout);
  return 0;
}

static int wifi_scan_command(int argc, char **argv)
{
  if (argc != 1)
  {
    return ESP_ERR_INVALID_ARG;
  }
  wifi_scan(NULL);
  return 0;
}