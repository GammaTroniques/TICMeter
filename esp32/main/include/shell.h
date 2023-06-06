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

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

void shellInit();

int esp_reset_command(int argc, char **argv);
esp_err_t esp_console_register_reset_command(void);

int get_wifi_command(int argc, char **argv);
int set_wifi_command(int argc, char **argv);
int connect_wifi_command(int argc, char **argv);
int reconnect_wifi_command(int argc, char **argv);
int wifi_disconnect_command(int argc, char **argv);
int wifi_status_command(int argc, char **argv);
esp_err_t esp_console_register_wifi_command(void);

int set_web_command(int argc, char **argv);
int get_web_command(int argc, char **argv);
esp_err_t esp_console_register_web_command();

int get_mqtt_command(int argc, char **argv);
int set_mqtt_command(int argc, char **argv);
int mqtt_connect_command(int argc, char **argv);
int mqtt_send_command(int argc, char **argv);
esp_err_t esp_console_register_mqtt_command();

int get_mode_command(int argc, char **argv);
int set_mode_command(int argc, char **argv);
esp_err_t esp_console_register_mode_command();

int get_config_command(int argc, char **argv);
int set_config_command(int argc, char **argv);
esp_err_t esp_console_register_config_command();

int get_VCondo_command(int argc, char **argv);
esp_err_t esp_console_register_VCondo_command();

#endif