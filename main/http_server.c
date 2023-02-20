#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lwip/inet.h"
#include "sys/param.h"

#include "http_server.h"
#include "tasks_common.h"
#include "wifi_app.h"

/* Tag user for esp serial console log */
static const char *TAG = "http_server";

/* Firmware update status */
static int g_fw_update_status = OTA_UPDATE_PENDING;

/* WiFi connect status */
static int g_wifi_connect_status = HTTP_WIFI_STATUS_NONE;
/* HTTP server task handle */
static httpd_handle_t http_server_handle = NULL;

/* HTTP server monitor task handle */
static TaskHandle_t task_server_http_monitor = NULL;

/* Queue handle used to manipulate the main queue of events */
static QueueHandle_t http_server_monitor_queue_handle = NULL;

/* Embedded files: JQuery, index.html, ap/css, app.js, favicon.ico files */
extern const uint8_t jquery_3_6_1_min_js_start[] asm("_binary_jquery_3_6_1_min_js_start");
extern const uint8_t jquery_3_6_1_min_js_end[] asm("_binary_jquery_3_6_1_min_js_end");

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

extern const uint8_t app_css_start[] asm("_binary_app_css_start");
extern const uint8_t app_css_end[] asm("_binary_app_css_end");

extern const uint8_t app_js_start[] asm("_binary_app_js_start");
extern const uint8_t app_js_end[] asm("_binary_app_js_end");

extern const uint8_t favicon_ico_start[] asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[] asm("_binary_favicon_ico_end");

/**
 * @brief ESP32 timer configuration passed to esp_timer_create
 */
const esp_timer_create_args_t fw_update_reset_args = {.callback = &http_server_fw_update_reset_callback,
                                                      .arg = NULL,
                                                      .dispatch_method = ESP_TIMER_TASK,
                                                      .name = "fw_update_reset"};
esp_timer_handle_t fw_update_reset;

/**
 * @brief Checks the g_fw_update_status and creates the fw_update_reset timer if g_fw_update_status is true.
 */
static void http_server_fw_update_timer()
{
    if (g_fw_update_status == OTA_UPDATE_SUCCESS)
    {
        ESP_LOGI(TAG, "http_server_fw_update_timer: FW updated successful, starting the FW update reset timer");

        /* Give the web-page a change to receive and acknowledge back and initialize the timer */
        ESP_ERROR_CHECK(esp_timer_create(&fw_update_reset_args, &fw_update_reset));
        const uint32_t timer_seconds_ms = 8 * 10000;
        ESP_ERROR_CHECK(esp_timer_start_once(fw_update_reset, timer_seconds_ms));
    }
    else
    {
        ESP_LOGI(TAG, "http_server_fw_update_timer: FW updated unsuccessful");
    }
}

/**
 * @brief HTTP server monitor task used to track events of the HTTP server
 *
 * @param pvParameter parameter which can be passed to task
 */
static void http_server_monitor(void *pvParameter)
{
    http_server_queue_message_t msg;
    for (;;)
    {
        if (xQueueReceive(http_server_monitor_queue_handle, &msg, portMAX_DELAY))
        {
            switch (msg.messageID)
            {
            case HTTP_MSG_WIFI_CONNECT_INIT: {
                ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_INIT");
                g_wifi_connect_status = HTTP_WIFI_STATUS_CONNECTING;
            }
            break;
            case HTTP_MSG_WIFI_CONNECT_SUCCESS: {
                ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_SUCCESS");
                g_wifi_connect_status = HTTP_WIFI_STATUS_CONNECT_SUCCESS;
            }
            break;
            case HTTP_MSG_WIFI_USER_DISCONNECT: {
                ESP_LOGI(TAG, "HTTP_MSG_USER_DISCONNECT");
                g_wifi_connect_status = HTTP_WIFI_STATUS_DISCONNECTED;
            }
            break;

            case HTTP_MSG_WIFI_CONNECT_FAIL: {
                ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_FAIL");
                g_wifi_connect_status = HTTP_WIFI_STATUS_CONNECT_FAIL;
            }
            break;
            case HTTP_MSG_OTA_UPDATE_SUCCESSFUL: {
                ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_SUCCESSFUL");
                g_fw_update_status = OTA_UPDATE_SUCCESS;
                http_server_fw_update_timer();
            }
            break;
            case HTTP_MSG_OTA_UPDATE_FAILED: {
                ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_FAILED");
                g_fw_update_status = OTA_UPDATE_FAILED;
            }
            break;

            default:
                break;
            }
        }
    }
}

/**
 * @brief Sends the message to monitor server queue
 *
 * @param messageID
 * @return BaseType_t
 */
BaseType_t http_server_monitor_send_message(http_server_message_e messageID)
{
    http_server_queue_message_t message = {.messageID = messageID};
    return xQueueSend(http_server_monitor_queue_handle, &message, portMAX_DELAY);
}

/**
 * @brief Jquery get handler requested when accessing to the web page.
 *
 * @param req HTTP request for which uri is need to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_jquery_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Jquery requested");
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char *)jquery_3_6_1_min_js_start, jquery_3_6_1_min_js_end - jquery_3_6_1_min_js_start);

    return ESP_OK;
}

/**
 * @brief Sends the index.html page.
 *
 * @param req HTTP request for which uri is need to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_index_html_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "index.html requested");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);

    return ESP_OK;
}

/**
 * @brief app.css get handler is requested when accessing to th web page.
 *
 * @param req HTTP request for which uri is need to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_app_css_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "app.css requested");
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (const char *)app_css_start, app_css_end - app_css_start);

    return ESP_OK;
}

/**
 * @brief app.js get handler is requested when accessing to th web page.
 *
 * @param req HTTP request for which uri is need to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_app_js_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "app.css requested");
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char *)app_js_start, app_js_end - app_js_start);

    return ESP_OK;
}

/**
 * @brief Sends the .ico (icon) file when accessing to the page
 *
 * @param req HTTP request for which uri is need to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_favicon_ico_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "favicon.ico requested");

    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_end - favicon_ico_start);

    return ESP_OK;
}

/**
 * @brief Receives the /bin file via the web page and handles the firmware update./
 *
 * @param req HTTP request for which uri is need to be handled.
 * @return ESP_OK, otherwise ESP_FAIL if timeout occurs and update can not be started
 */
static esp_err_t http_server_OTA_update_handler(httpd_req_t *req)
{
    esp_ota_handle_t ota_handle;

    uint32_t buffer_size = 1024;
    char ota_buff[buffer_size];
    uint32_t content_length = req->content_len;
    int32_t receive_len;
    uint32_t content_received = 0;
    bool is_request_body_started = false;

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL)
    {
        ESP_LOGI(TAG, "http_server_OTA_update_handler: INVALID PTA PARTITION");
        return ESP_FAIL;
    }

    do
    {
        /* Read the data for the request */
        receive_len = httpd_req_recv(req, ota_buff, MIN(content_length, buffer_size));
        if (receive_len < 0)
        {
            /* Check if timeout ocurred */
            if (receive_len == HTTPD_SOCK_ERR_TIMEOUT)
            {
                ESP_LOGI(TAG, "http_server_OTA_update_handler: socket timeout");
                continue; /* >Retry receiving if timeout occurred */
            }
            ESP_LOGI(TAG, "http_server_OTA_update_handler: OTA other error");
            return ESP_FAIL;
        }

        /* If it is first data we are receiving.
         * If so, it will have information in the header that we need.
         */
        if (!is_request_body_started)
        {
            is_request_body_started = true;

            char *body_start_p = strstr(ota_buff, "\r\n\r\n") + 4;
            uint32_t body_part_len = receive_len - (body_start_p - ota_buff);

            printf("http_server_OTA_update_handler: OTA file_size: %ld\n", content_length);
            esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);

            if (err != ESP_OK)
            {
                printf("http_server_OTA_update_handler: Error with OTA begin, canceling the OTA");
                return ESP_FAIL;
            }
            else
            {
                printf("http_server_OTA_update_handler: Writing to partition subtype %d at offset 0x%lx\r\n",
                       update_partition->subtype, update_partition->address);
            }

            /* Write the first part of the data */
            esp_ota_write(ota_handle, body_start_p, body_part_len);
            content_received += body_part_len;
        }
        else
        {
            /* Write OTA data */
            esp_ota_write(ota_handle, ota_buff, receive_len);
            content_received += receive_len;
        }
    } while (receive_len > 0 && content_received < content_length);

    if (esp_ota_end(ota_handle) != ESP_OK)
    {
        ESP_LOGI(TAG, "http_server_OTA_update_handler: esp_ota_end ERROR");
        http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_FAILED);
        return ESP_OK;
    }

    if (esp_ota_set_boot_partition(update_partition) != ESP_OK)
    {
        ESP_LOGI(TAG, "http_server_OTA_update_handler: Flash ERROR");
        http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_FAILED);
        return ESP_OK;
    }

    const esp_partition_t *boot_partition = esp_ota_get_boot_partition();
    ESP_LOGI(TAG, "http_server_OTA_update_handler: Next boot partition subtype %d at offset 0x%lx",
             boot_partition->subtype, boot_partition->address);
    http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_SUCCESSFUL);
    return ESP_OK;
}

/**
 * @brief OTA status handler responds with the firmware update status after the OTA update is started \
 * and responds with the compile time/date when the page is first requested.
 *
 * @param req HTTP request for which uri is need to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_OTA_status_handler(httpd_req_t *req)
{
    char otaJSON[128];

    ESP_LOGI(TAG, "OTAstatus requested");
    sprintf(otaJSON, "{\"ota_update_status\":%d,\"compile_time\":\"%s\",\"compile_date\":\"%s\"}", g_fw_update_status,
            __TIME__, __DATE__);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, otaJSON, strlen(otaJSON));

    return ESP_OK;
}

/**
 * @brief Get the value from HTTP request header.
 *
 * @param req HTTP request for which uri is need to be handled.
 * @param field header field that should be found and returned.
 * @return allocated char* if information was found, otherwise NULL;
 */
static char *get_value_from_header(httpd_req_t *req, char *field)
{
    size_t len = httpd_req_get_hdr_value_len(req, field) + 1;
    char *str = NULL;
    if (len > 1)
    {
        str = malloc(len);
        if (httpd_req_get_hdr_value_str(req, field, str, len) == ESP_OK)
        {
            ESP_LOGI(TAG, "get_value_from_header: Found header -> %s: %s", field, str);
        }
    }
    return str;
}

/**
 * @brief wifiConnect.json handler is invoked after the connection button is pressed.
 *
 * @param req HTTP request for which uri is need to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_wifi_connect_json_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "wifiConnect.json requested");

    char *ssid_str = get_value_from_header(req, "my-connect-ssid");
    char *pwd_str = get_value_from_header(req, "my-connect-pwd");

    /* Update the WiFi network configuration and let the wifi app know */
    wifi_config_t *wifi_config = wifi_app_get_wifi_config();
    memset(wifi_config, 0x00, sizeof(wifi_config_t));
    memcpy(wifi_config->sta.ssid, ssid_str, strlen(ssid_str) + 1);
    memcpy(wifi_config->sta.password, pwd_str, strlen(pwd_str) + 1);

    free(ssid_str);
    free(pwd_str);

    wifi_app_send_message(WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER);
    return ESP_OK;
}

/**
 * @brief wifiDisconnect.json handler responds by sending a message to the WiFi app.
 *
 * @param req HTTP request for which uri is need to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_wifi_disconnect_json_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "wifiDisconnect.json requested");

    wifi_app_send_message(WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT);
    return ESP_OK;
}

/**
 * @brief Update the connect status for the web page
 *
 * @param req HTTP request for which uri is need to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_wifi_connect_status_json_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "/wifiConnectStatus requested");
    char statusJSON[30];
    sprintf(statusJSON, "{\"wifi_connect_status\":%d}", g_wifi_connect_status);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, statusJSON, strlen(statusJSON));

    return ESP_OK;
}

/**
 * @brief WifiConnection.json handler update the web-page with connection information.
 *
 * @param req HTTP request for which uri is need to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_get_wifi_connect_info_json_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "/wifiConnectInfo.json requested");

    char ipInfoJSON[200];
    memset(ipInfoJSON, 0, sizeof(ipInfoJSON));

    char ip[IP4ADDR_STRLEN_MAX];
    char netmask[IP4ADDR_STRLEN_MAX];
    char gw[IP4ADDR_STRLEN_MAX];

    if (g_wifi_connect_status == HTTP_WIFI_STATUS_CONNECT_SUCCESS)
    {
        wifi_ap_record_t wifi_data;
        ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&wifi_data));
        char *ssid = (char *)wifi_data.ssid;

        esp_netif_ip_info_t ip_info;
        ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_sta, &ip_info));
        esp_ip4addr_ntoa(&ip_info.ip, ip, IP4ADDR_STRLEN_MAX);
        esp_ip4addr_ntoa(&ip_info.netmask, netmask, IP4ADDR_STRLEN_MAX);
        esp_ip4addr_ntoa(&ip_info.gw, gw, IP4ADDR_STRLEN_MAX);

        sprintf(ipInfoJSON, "{\"ip\":\"%s\",\"netmask\":\"%s\",\"gw\":\"%s\",\"ap\":\"%s\"}", ip, netmask, gw, ssid);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, ipInfoJSON, strlen(ipInfoJSON));

    return ESP_OK;
}

/**
 * @brief Creates and registers uri handler on HTTP server
 *
 * @param uri uri that should be registered.
 * @param method HTTP method.
 * @param handler uri handler.
 * @param user_ctx user information.
 */
static void http_server_create_and_register_uri_handle(const char *uri, enum http_method method,
                                                       esp_err_t (*handler)(httpd_req_t *r), void *user_ctx)
{
    httpd_uri_t _httpd_uri = {
        .uri = uri,
        .method = method,
        .handler = handler,
        .user_ctx = user_ctx,
    };
    httpd_register_uri_handler(http_server_handle, &_httpd_uri);
}

/**
 * @brief Sets up the http server configuration
 *
 * @return http instance handle if success, otherwise NULL
 */
static httpd_handle_t http_server_configure()
{
    /* Generate default configuration */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    /* Create http sever monitor task */
    xTaskCreatePinnedToCore(&http_server_monitor, "http_server_monitor", HTTP_SERVER_MONITOR_SIZE, NULL,
                            HTTP_SERVER_MONITOR_PRIORITY, &task_server_http_monitor, HTTP_SERVER_MONITOR_CORE_ID);
    /* Create message queue */
    http_server_monitor_queue_handle = xQueueCreate(3, sizeof(http_server_queue_message_t));
    /* The core that the HTTP server will run on */
    config.core_id = HTTP_SERVER_TASK_CODE_ID;
    config.task_priority = HTTP_SERVER_TASK_PRIORITY;
    config.stack_size = HTTP_SERVER_TASK_SIZE;
    config.max_uri_handlers = 20;

    uint16_t receive_wait_timeout_s = 10;
    uint16_t send_wait_timeout_s = 10;

    config.recv_wait_timeout = receive_wait_timeout_s;
    config.send_wait_timeout = send_wait_timeout_s;

    ESP_LOGI(TAG, "http_server_configure: starting server on port: %d, with task priority: %d", config.server_port,
             config.task_priority);

    if (httpd_start(&http_server_handle, &config) != ESP_OK)
    {
        return NULL;
    }

    ESP_LOGI(TAG, "http_server_configure: Registering URI handlers");

    /* Register URI handlers */
    http_server_create_and_register_uri_handle("/jquery-3.6.1.min.js", HTTP_GET, http_server_jquery_handler, NULL);
    http_server_create_and_register_uri_handle("/", HTTP_GET, http_server_index_html_handler, NULL);
    http_server_create_and_register_uri_handle("/app.css", HTTP_GET, http_server_app_css_handler, NULL);
    http_server_create_and_register_uri_handle("/app.js", HTTP_GET, http_server_app_js_handler, NULL);
    http_server_create_and_register_uri_handle("/favicon.ico", HTTP_GET, http_server_favicon_ico_handler, NULL);
    http_server_create_and_register_uri_handle("/OTAupdate", HTTP_POST, http_server_OTA_update_handler, NULL);
    http_server_create_and_register_uri_handle("/OTAstatus", HTTP_POST, http_server_OTA_status_handler, NULL);
    http_server_create_and_register_uri_handle("/wifiConnect.json", HTTP_POST, http_server_wifi_connect_json_handler,
                                               NULL);
    http_server_create_and_register_uri_handle("/wifiDisconnect.json", HTTP_DELETE,
                                               http_server_wifi_disconnect_json_handler, NULL);
    http_server_create_and_register_uri_handle("/wifiConnectStatus", HTTP_POST,
                                               http_server_wifi_connect_status_json_handler, NULL);
    http_server_create_and_register_uri_handle("/wifiConnectInfo.json", HTTP_GET,
                                               http_server_get_wifi_connect_info_json_handler, NULL);
    return http_server_handle;
}

void http_server_start()
{
    if (http_server_handle == NULL)
    {
        http_server_handle = http_server_configure();
    }
}

void http_server_stop()
{
    if (http_server_handle != NULL)
    {
        httpd_stop(http_server_handle);
        ESP_LOGI(TAG, "http_server_stop: stopping HTTP server");
    }
    if (task_server_http_monitor)
    {
        vTaskDelete(task_server_http_monitor);
        ESP_LOGI(TAG, "http_server_stop: stopping HTTP server monitor");
        task_server_http_monitor = NULL;
    }
}

void http_server_fw_update_reset_callback(void *arg)
{
    ESP_LOGI(TAG, "http_server_fw_update_reset_callback: Timer timed-out, restarting the devise");
    esp_restart();
}
