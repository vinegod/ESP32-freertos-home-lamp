#ifndef WIFI_APP_H_
#define WIFI_APP_H_

#include "freertos/FreeRTOS.h"

#include "esp_netif.h"
#include "esp_wifi.h"
#include "led_strip.h"

#define WIFI_AP_SSID "ESP32_AP"
#define WIFI_AP_PASSWORD "password"
#define WIFI_AP_CHANNEL 1
#define WIFI_AP_SSID_HIDDEN 0
#define WIFI_AP_MAX_CONNECTIONS 5
#define WIFI_AP_BEACON_INTERVAL 100
#define WIFI_AP_IP "192.168.0.99"
#define WIFI_AP_GATEWAY "192.168.0.99"
#define WIFI_AP_NETMASK "255.255.255.0"
#define WIFI_AP_BANDWIDTH WIFI_BW_HT20
#define WIFI_STA_POWER_SAVE WIFI_PS_NONE
#define MAX_SSID_LENGTH 32
#define MAX_PASSWORD_LENGTH 64
#define MAX_CONNECTIONS_RETRIES 5

extern esp_netif_t *esp_netif_sta;
extern esp_netif_t *esp_netif_ap;

/**
 * @brief Message ID's for WIFI application tasks
 */
typedef enum
{
    WIFI_APP_MSG_START_HTTP_SERVER = 0,
    WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER,
    WIFI_APP_MSG_STA_CONNECTED_GOT_IP,
    WIFI_APP_MSG_STA_LOAD_SAVED_CREDENTIALS,
    WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT,
    WIFI_APP_MSG_STA_DISCONNECTED,
} wifi_app_message_e;

/**
 * @brief Structure for the message queue
 */
typedef struct
{
    wifi_app_message_e messageID;
} wifi_app_queue_message_t;

/**
 * @brief fSends a message tp the queue
 *
 * @param msgID message ID from the wifi_app_message_e enum
 * @return pdTRUE if an item was successfully, otherwise pdFALSE
 */
BaseType_t wifi_app_send_message(wifi_app_message_e messageID);

/**
 * @brief Starts thw WIFI RTOS task
 */
void wifi_app_start(led_strip_handle_t led_strip);

/**
 * @brief Get the WiFi configuration
 *
 * @return wifi_config_t* pointer to configuration
 */
wifi_config_t *wifi_app_get_wifi_config();

#endif /* WIFI_APP_H_ */
