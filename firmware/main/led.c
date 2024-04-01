/**
 * @file led.c
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
#include <led_strip.h>

#include "led.h"
#include "gpio.h"
#include "linky.h"
#include "config.h"
/*==============================================================================
 Local Define
===============================================================================*/
#define PATTERN_SIZE 3

/*==============================================================================
 Local Macro
===============================================================================*/
#define TAG "GPIO"

#define FOREVER UINT32_MAX

/*==============================================================================
 Local Type
===============================================================================*/
typedef enum
{
    LED_FLASH,
    LED_FLASH_MODE,
    LED_WAVE,
    LED_MANUAL,
} led_type_t;

typedef struct led_timing_t
{
    const uint32_t id;
    const uint16_t priority;
    const led_type_t type;
    const uint32_t color;
    const uint32_t tOn;
    const uint32_t tOff;
    const uint32_t repeat;
    const uint8_t only_usb_powered;
    uint8_t in_progress;
} led_timing_t;

/*==============================================================================
 Local Function Declaration
===============================================================================*/
static void led_pattern_task(void *pattern_ptr);
static void led_task(void *pvParameters);
static void led_set_rgb(uint32_t color, uint32_t brightness);
static void led_start_next_pattern();

/*==============================================================================
Public Variable
===============================================================================*/

const uint32_t led_color_mode[] = {
    [MODE_NONE] = 0x000000,
    [MODE_WEB] = 0x0008FF,
    [MODE_MQTT] = 0x8803FC,
    [MODE_MQTT_HA] = 0x8803FC,
    [MODE_ZIGBEE] = 0xFF0000,
    [MODE_MATTER] = 0xFFFFFF,
    [MODE_TUYA] = 0xB04000,
};
/*==============================================================================
 Local Variable
===============================================================================*/

// clang-format off
static led_timing_t led_timing[] = {
    {LED_FACTORY_RESET,         103,    LED_FLASH,      0x00F0FF,                      2000,    100,    FOREVER, 0, 0,},
    {LED_COLOR_WHEEL,           102,    LED_MANUAL,     0x000000,                         0,    0,      FOREVER, 0, 0,},
    {LED_FACTORY_RESET_ADVERT,  101,    LED_FLASH,      0x00F0FF,                       100,    100,    FOREVER, 0, 0,},
    {LED_BOOT,                  100,    LED_FLASH_MODE, 0x000000,                       100,    0,      1,       0, 0,},
    {LED_PAIRING,               99,     LED_FLASH_MODE, 0x000000,                       100,    900,    FOREVER, 0, 0,},

    {LED_CONNECTING,            60,     LED_FLASH_MODE, 0x000000,                       100,    900,    FOREVER, 0, 0,},
    {LED_CONNECTING_FAILED,     61,     LED_FLASH,      0xFF0000,                       50,     100,    4,       0, 0,},
    
    {LED_SENDING,               70,     LED_FLASH_MODE, 0x000000,                       100,    900,    FOREVER, 0, 0,},
    {LED_SEND_FAILED,           72,     LED_FLASH,      0xFF0000,                       50,     100,    5,       0, 0,},
    {LED_SEND_OK,               71,     LED_FLASH,      0x00FF00,                       300,    0,      1,       0, 0,},

    {LED_LINKY_READING,         50,     LED_FLASH,      0xFF8000,                       100,    900,    FOREVER, 0, 0,},
    {LED_LINKY_FAILED,          51,     LED_FLASH,      0xFF0000,                       50,     100,    3,       0, 0,},
    
    {LED_NO_CONFIG,             20,     LED_FLASH,      0xFF0000,                       50,     100,    2,       0, 0,},
    
    {LED_OTA_AVAILABLE,         10,     LED_WAVE,       0x0000FF,                       0,      1000,   FOREVER, 1, 0,},
    {LED_OTA_IN_PROGRESS,       11,     LED_WAVE,       0xFFFF00,                       0,      1000,   FOREVER, 1, 0,},

};

static const uint8_t led_pattern_size = sizeof(led_timing) / sizeof(led_timing_t);

static led_strip_handle_t led;
static TaskHandle_t led_task_handle = NULL;
static QueueHandle_t led_pattern_queue = NULL;
static TaskHandle_t  led_pattern_task_handle = NULL;
static led_timing_t *led_current_pattern = NULL;
// clang-format on

/*==============================================================================
Function Implementation
===============================================================================*/

uint32_t led_init()
{
    gpio_set_direction(LED_EN, GPIO_MODE_OUTPUT);
    // gpio_set_direction(LED_DATA, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_EN, 1);

    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_DATA,               // The GPIO that connected to the LED strip's data line
        .max_leds = 1,                            // The number of LEDs in the strip,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
        .led_model = LED_MODEL_WS2812,            // LED strip model
        .flags = {
            .invert_out = false, // whether to invert the output signal (useful when your hardware has a level inverter)
        },
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,    // different clock source can lead to different power consumption
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .mem_block_symbols = 0,
        .flags = {
            .with_dma = false, // whether to enable the DMA feature
        },
    };

    if (led)
    {
        led_strip_del(led);
    }
    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &led);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "LED init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    led_strip_clear(led);
    led_strip_refresh(led);

    if (led_pattern_queue == NULL)
    {
        ESP_LOGD(TAG, "Creating led_pattern_queue");
        led_pattern_queue = xQueueCreate(15, sizeof(led_pattern_t));
    }
    if (led_task_handle == NULL)
    {
        xTaskCreate(led_task, "led_task", 4096, NULL, PRIORITY_LED, &led_task_handle);
    }

    return 0;
}

/**
 * @brief
 *
 * @param color
 * @param brightness in per thousand
 */
static void led_set_rgb(uint32_t color, uint32_t brightness)
{

    if (color == 0 || brightness == 0)
    {
        led_strip_clear(led);
        led_strip_refresh(led);
        vTaskDelay(1);
        gpio_set_level(LED_EN, 0);
        // gpio_set_direction(LED_DATA, GPIO_MODE_INPUT); // HIGH-Z
        return;
    }

    uint32_t r = (color >> 16) & 0xFF;
    uint32_t g = (color >> 8) & 0xFF;
    uint32_t b = (color >> 0) & 0xFF;

    // set brightness
    r = (r * brightness) / 1000;
    g = (g * brightness) / 1000;
    b = (b * brightness) / 1000;

    ESP_LOGD(TAG, "r: %ld, g: %ld, b: %ld, brightness: %ld", r, g, b, brightness);
    gpio_set_level(LED_EN, 1);
    led_strip_set_pixel(led, 0, r, g, b);
    led_strip_refresh(led);
}

void led_set_color(uint32_t color)
{
    led_set_rgb(color, 50); // 5% brightness
    // gpio_set_direction(LED_DATA, GPIO_MODE_OUTPUT);
}

static void led_task(void *pvParameters)
{

    led_pattern_t pattern;
    while (1)
    {
        xQueueReceive(led_pattern_queue, &pattern, portMAX_DELAY);
        ESP_LOGD(TAG, "pattern: %d", pattern);
        led_timing_t *timing = NULL;

        for (int i = 0; i < led_pattern_size; i++)
        {
            if (led_timing[i].id == pattern)
            {
                timing = &led_timing[i];
                break;
            }
        }
        if (timing == NULL)
        {
            ESP_LOGE(TAG, "Pattern not found: %d", pattern);
            continue;
        }

        uint32_t most_priority = 0;
        for (int i = 0; i < led_pattern_size; i++)
        {
            if (led_timing[i].in_progress)
            {
                if (led_timing[i].priority > timing->priority)
                {
                    most_priority = led_timing[i].priority;
                }
            }
        }

        if (most_priority >= timing->priority)
        {
            ESP_LOGD(TAG, "Pattern %d not started because of priority", pattern);
            continue;
        }

        if (timing->in_progress)
        {
            ESP_LOGD(TAG, "Pattern %d already in progress", pattern);
            continue;
        }

        if (led_current_pattern != NULL && led_current_pattern->in_progress)
        {
            // led_current_pattern->in_progress = 0;
            deleteTask(led_pattern_task_handle);
            led_set_color(0);
            vTaskDelay(200 / portTICK_PERIOD_MS);
        }
        timing->in_progress = 1;
        led_current_pattern = timing;
        xTaskCreate(led_pattern_task, "led_pattern_task", 4 * 1024, timing, PRIORITY_LED_PATTERN, &led_pattern_task_handle);
        ESP_LOGD(TAG, "Pattern %d started", pattern);
    }
}

static void led_pattern_task(void *pattern_ptr)
{
    led_timing_t *pattern = (led_timing_t *)pattern_ptr;
    if (pattern == NULL)
    {
        ESP_LOGE(TAG, "Pattern is NULL");
        vTaskDelete(NULL);
    }
    ESP_LOGW(TAG, "Pattern %ld, type: %d, color: %ld, tOn: %ld, tOff: %ld, repeat: %ld", pattern->id, pattern->type, pattern->color, pattern->tOn, pattern->tOff, pattern->repeat);
    uint32_t repeat = pattern->repeat;
    while (repeat == FOREVER || repeat > 0)
    {
        switch (pattern->type)
        {
        case LED_FLASH:
            led_set_color(pattern->color);
            vTaskDelay(pattern->tOn / portTICK_PERIOD_MS);
            led_set_color(0);
            vTaskDelay(pattern->tOff / portTICK_PERIOD_MS);
            if (repeat != FOREVER)
            {
                repeat--;
            }
            break;
        case LED_FLASH_MODE:
            led_set_color(led_color_mode[config_values.mode]);
            vTaskDelay(pattern->tOn / portTICK_PERIOD_MS);
            led_set_color(0);
            vTaskDelay(pattern->tOff / portTICK_PERIOD_MS);
            if (repeat != FOREVER)
            {
                repeat--;
            }
            break;
        case LED_WAVE:
            uint32_t brightness = 0;
            while (brightness < 500)
            {
                brightness++;
                led_set_rgb(pattern->color, brightness);
                vTaskDelay(5 / portTICK_PERIOD_MS);
            }
            vTaskDelay(pattern->tOn / portTICK_PERIOD_MS);
            while (brightness > 0)
            {
                brightness--;
                led_set_rgb(pattern->color, brightness);
                vTaskDelay(5 / portTICK_PERIOD_MS);
            }
            vTaskDelay(pattern->tOff / portTICK_PERIOD_MS);
            if (repeat != FOREVER)
            {
                repeat--;
            }
            break;
        case LED_MANUAL:
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            break;
        default:
            ESP_LOGE(TAG, "Unknown pattern type");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            break;
        }
    }
    led_set_color(0);
    pattern->in_progress = 0;

    vTaskDelay(100 / portTICK_PERIOD_MS);
    led_start_next_pattern();
    vTaskDelete(NULL); // Delete this task
}

void led_start_pattern(led_pattern_t pattern)
{
    if (led_pattern_queue == NULL)
    {
        ESP_LOGE(TAG, "led_pattern_queue is NULL");
        return;
    }
    xQueueSend(led_pattern_queue, &pattern, 0);
}

void led_stop_pattern(led_pattern_t pattern)
{
    for (int i = 0; i < led_pattern_size; i++)
    {
        if (led_timing[i].id == pattern && led_timing[i].in_progress)
        {
            led_timing[i].in_progress = 0;
            if (led_current_pattern != NULL)
            {
                led_current_pattern->in_progress = 0;
                deleteTask(led_pattern_task_handle);
                led_current_pattern = NULL;
            }
            return;
        }
    }
    ESP_LOGW(TAG, "Can't stop pattern %d, not in progress", pattern);
}

void led_usb_event(bool connected)
{
    if (connected)
    {
    }
    else
    {
        // stop all patterns
        for (int i = 0; i < led_pattern_size; i++)
        {
            if (led_timing[i].in_progress && led_timing[i].only_usb_powered)
            {
                led_timing[i].in_progress = 0;
            }
        }
        if (led_current_pattern != NULL && led_current_pattern->only_usb_powered)
        {
            led_current_pattern->in_progress = 0;
            deleteTask(led_pattern_task_handle);
            led_current_pattern = NULL;
            led_set_color(0);
        }
    }
}

static void led_start_next_pattern()
{
    uint16_t most_priority = 0;

    // find most priority pattern
    for (int i = 0; i < led_pattern_size; i++)
    {
        if (led_timing[i].in_progress && led_timing[i].repeat == FOREVER)
        {
            if (led_timing[i].priority > most_priority)
            {
                most_priority = led_timing[i].priority;
            }
        }
    }

    // start most priority pattern
    for (int i = 0; i < led_pattern_size; i++)
    {
        if (led_timing[i].priority == most_priority)
        {
            led_start_pattern(led_timing[i].id);
            return;
        }
    }
}