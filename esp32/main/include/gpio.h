
#ifndef __GPIO_H__
#define __GPIO_H__
#include <stdio.h>

#define V_CONDO_PIN ADC_CHANNEL_5
#define V_USB_PIN (gpio_num_t)3
#define PAIRING_PIN (gpio_num_t)6 // io6
#define PAIRING_LED_PIN (gpio_num_t)11
#define LED_RED (gpio_num_t)7
#define LED_GREEN (gpio_num_t)11

void initPins();
uint8_t getVUSB();
float getVCondo();
void led_blink_task(void *pvParameter);
void pairingButtonTask(void *pvParameter);
void startLedPattern(gpio_num_t led, uint8_t count, uint16_t tOn, uint16_t tOff);
void loop(void *arg);
#endif