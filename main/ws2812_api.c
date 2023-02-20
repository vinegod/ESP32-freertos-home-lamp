#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"

#include "ws2812_api.h"

esp_err_t init_ws2812(led_strip_handle_t *led_strip)
{
    led_strip_config_t config = {.strip_gpio_num = LED, .max_leds = MAX_LEDS};
    led_strip_rmt_config_t rmt_config = {.resolution_hz = 10000000};
    return led_strip_new_rmt_device(&config, &rmt_config, led_strip);
}

void enable_white_light(led_strip_handle_t led_strip)
{
    for (uint32_t i = 0; i < MAX_LEDS; ++i)
    {
        led_strip_set_pixel(led_strip, i, 253, 227, 198);
    }
    led_strip_refresh(led_strip);
    vTaskDelay(100);
    led_strip_clear(led_strip);
}

static void _enable_light(led_strip_handle_t led_strip, rgb_color_t color_parameters)
{
    led_strip_clear(led_strip);
    for (uint32_t i = 0; i < MAX_LEDS; ++i)
    {
        led_strip_set_pixel(led_strip, i, color_parameters.color_rgb.red, color_parameters.color_rgb.green,
                            color_parameters.color_rgb.blue);
    }
    led_strip_refresh(led_strip);
}

void enable_light(led_strip_handle_t led_strip, rgb_color_t color)
{
    _enable_light(led_strip, color);
}

void enable_light_color(led_strip_handle_t led_strip, color_e color)
{
    enable_light(led_strip, color_to_rgb_struct(color));
}

void disable_light(led_strip_handle_t led_strip)
{
    led_strip_clear(led_strip);
}
