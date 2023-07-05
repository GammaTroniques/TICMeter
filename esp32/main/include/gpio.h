
#ifndef __GPIO_H__
#define __GPIO_H__
#include <stdio.h>

#define V_CONDO_PIN ADC_CHANNEL_5
#define V_USB_PIN (gpio_num_t)3
#define PAIRING_PIN (gpio_num_t)6 // io6
#define PAIRING_LED_PIN (gpio_num_t)11
#define LED_RED (gpio_num_t)7
#define LED_GREEN (gpio_num_t)11

#define PATTERN_WIFI_CONNECTING 0
#define PATTERN_WIFI_RETRY 1
#define PATTERN_WIFI_FAILED 2
#define PATTERN_LINKY_OK 3
#define PATTERN_LINKY_ERR 4
#define PATTERN_SEND_OK 5
#define PATTERN_SEND_ERR 6
#define PATTERN_NO_CONFIG 7
#define PATTERN_START 8

void initPins();
uint8_t getVUSB();
float getVCondo();
void led_blink_task(void *pvParameter);
void pairingButtonTask(void *pvParameter);
void startLedPattern(uint8_t pattern);
void loop(void *arg);
#endif