#include "nvs_flash.h"

#include "ws2812_api.h"
#include "wifi_app.h"

#define GPIO_INPUT_BUTTON 35
#define GPIO_INPUT_BITMASK (1ULL << GPIO_INPUT_BUTTON)

void app_main(void)
{

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    led_strip_handle_t led_strip = {NULL};
    ESP_ERROR_CHECK(init_ws2812(&led_strip));
    // Start WiFi
    wifi_app_start(led_strip);
}
