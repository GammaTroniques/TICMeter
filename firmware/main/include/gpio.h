
#ifndef __GPIO_H__
#define __GPIO_H__
#include <stdio.h>

#define RX_LINKY 17
#define V_CONDO_PIN ADC_CHANNEL_4
#define V_USB_PIN ADC_CHANNEL_1
// #define PAIRING_PIN (gpio_num_t)3
#define PAIRING_PIN (gpio_num_t)9
#define BOOT_PIN (gpio_num_t)9

#define LED_EN (gpio_num_t)0
#define LED_DATA (gpio_num_t)5

#define PAIRING_LED_PIN (gpio_num_t)23 // 23 --> unused
#define LED_RED (gpio_num_t)23
#define LED_GREEN (gpio_num_t)23

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

float getVUSB();

/**
 * @brief Get the tension of the condo
 *
 * @return float
 */
float getVCondo();
void pairingButtonTask(void *pvParameter);
void startLedPattern(uint8_t pattern);
void noConfigLedTask(void *pvParameters);
void wifiConnectLedTask(void *pvParameters);
void linkyReadingLedTask(void *pvParameters);
void sendingLedTask(void *pvParameters);
/**
 * @brief set the CPU frequency to the given value
 *
 */
void setCPUFreq(int32_t speedInMhz);

#endif