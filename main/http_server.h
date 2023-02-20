#ifndef HTTP_SERVER_H_
#define HTTP_SERVER_H_

#include "freertos/FreeRTOS.h"

#define OTA_UPDATE_PENDING 0
#define OTA_UPDATE_SUCCESS 1
#define OTA_UPDATE_FAILED -1

/**
 * @brief Messages for HTTP monitor
 */
typedef enum http_server_message
{
    HTTP_MSG_WIFI_CONNECT_INIT = 0,
    HTTP_MSG_WIFI_CONNECT_SUCCESS,
    HTTP_MSG_WIFI_CONNECT_FAIL,
    HTTP_MSG_WIFI_USER_DISCONNECT,
    HTTP_MSG_OTA_UPDATE_SUCCESSFUL,
    HTTP_MSG_OTA_UPDATE_FAILED,
} http_server_message_e;

/**
 * @brief Wifi connection states
 */
typedef enum http_server_wifi_connect_status
{
    HTTP_WIFI_STATUS_NONE = 0,
    HTTP_WIFI_STATUS_CONNECTING,
    HTTP_WIFI_STATUS_CONNECT_FAIL,
    HTTP_WIFI_STATUS_CONNECT_SUCCESS,
    HTTP_WIFI_STATUS_DISCONNECTED,
} http_server_wifi_connect_status_e;

/**
 * @brief Structure for message queue
 */
typedef struct http_server_queue_message
{
    http_server_message_e messageID;
} http_server_queue_message_t;

/**
 * @brief Sends a message to a queue
 *
 * @param messageID message ID from the http_server_message_e enum
 * @return pdTRUE if an item was successfully send to a queue, otherwise pdFALSE
 */
BaseType_t http_server_monitor_send_message(http_server_message_e messageID);

/**
 * @brief Starts HTTP server
 */
void http_server_start();

/**
 * @brief Stops HTTP sever
 */
void http_server_stop();

/**
 * @brief Timer callback function which calls esp_restart upon successful firmware update
 *
 * @param arg
 */
void http_server_fw_update_reset_callback(void *arg);

#endif /* HTTP_SERVER_H_ */
