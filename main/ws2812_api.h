#ifndef LED_STRIP_WS2812_H_
#define LED_STRIP_WS2812_H_ 

#include "led_strip.h"
#include "colors.h"

#define LED 25
#define MAX_LEDS 15

/**
 * @brief Init WS2812 led strip
 *
 * @param led_strip pointer on initiated strip
 * @return
 *      - ESP_OK: create LED strip handle successfully
 *      - ESP_ERR_INVALID_ARG: create LED strip handle failed because of invalid argument
 *      - ESP_ERR_NO_MEM: create LED strip handle failed because of out of memory
 *      - ESP_FAIL: create LED strip handle failed because some other error
 */
esp_err_t init_ws2812(led_strip_handle_t *led_strip);

/**
 * @brief Write "warm white" color to led strip and refreshes its state
 *
 * @param led_strip pointer on initiated strip
 */
void enable_white_light(led_strip_handle_t led_strip);

void enable_light(led_strip_handle_t led_strip, rgb_color_t color);
void enable_light_color(led_strip_handle_t led_strip, color_e color);
void disable_light(led_strip_handle_t led_strip);

#endif /* LED_STRIP_WS2812_H_ */