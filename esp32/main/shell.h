#ifndef SHELL_H
#define SHELL_H
#include "config.h"
// #include "mqtt.h"
#include "driver/uart.h"
#include "esp_console.h"
#include "string.h"
#include "esp_log.h"
#include "argtable3/argtable3.h"
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-guides/usb-serial-jtag-console.html

struct shell_t
{
    Config *config;
    // Mqtt *mqtt;
};

void shellTask(void *pvParameters);

static int get_wifi_command(int argc, char **argv);
static int set_wifi_command(int argc, char **argv);
static int connect_wifi_command(int argc, char **argv);
static int wifi_disconnect_command(int argc, char **argv);
static int wifi_status_command(int argc, char **argv);
esp_err_t esp_console_register_wifi_command(void);

static int set_web_command(int argc, char **argv);
static int get_web_command(int argc, char **argv);
esp_err_t esp_console_register_web_command();

static int get_mqtt_command(int argc, char **argv);
static int set_mqtt_command(int argc, char **argv);
static int mqtt_connect_command(int argc, char **argv);
static int mqtt_send_command(int argc, char **argv);
esp_err_t esp_console_register_mqtt_command();

static int get_mode_command(int argc, char **argv);
static int set_mode_command(int argc, char **argv);
esp_err_t esp_console_register_mode_command();

static int get_config_command(int argc, char **argv);
static int set_config_command(int argc, char **argv);
esp_err_t esp_console_register_config_command();

#endif