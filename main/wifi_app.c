#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "lwip/netdb.h"

#include "app_nvs.h"
#include "http_server.h"
#include "tasks_common.h"
#include "wifi_app.h"
#include "ws2812_api.h"

// Tag used for ESP serial console messages
static const char *TAG = "wifi_app";

/* Used for returning WiFi configuration */
wifi_config_t *wifi_config = NULL;

// Queue handle used to manipulate the main queue of events
static QueueHandle_t wifi_app_queue_handle;

// netif objects for the station and access point
esp_netif_t *esp_netif_sta = NULL;
esp_netif_t *esp_netif_ap = NULL;

/* Numbers of WiFI STA connecting tries */
static int g_retry_number;

/* WiFi application event group handle and status bits */
static EventGroupHandle_t wifi_app_event_group;
const int WIFI_APP_MSG_STA_LOAD_SAVED_CREDENTIALS_BIT = BIT0;
const int WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER_BIT = BIT1;
const int WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT_BIT = BIT2;

/**
 * @brief Wifi app event handler
 *
 * @param arg aside to the event data, this is passed to handler when its called
 * @param event_base the base id of the event to register handler for
 * @param event_id the id of the event to register the handle for
 * @param event_data event data
 */
static void wifi_wifi_app_event_group(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_AP_START:
            ESP_LOGI(TAG, "WIFI_EVENT_AP_START");
            break;

        case WIFI_EVENT_AP_STOP:
            ESP_LOGI(TAG, "WIFI_EVENT_AP_STOP");
            break;

        case WIFI_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "WIFI_EVENT_AP_STACONNECTED");
            break;

        case WIFI_EVENT_AP_STADISCONNECTED:
            ESP_LOGI(TAG, "WIFI_EVENT_AP_STADISCONNECTED");
            break;

        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
            break;

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");
            if (g_retry_number >= MAX_CONNECTIONS_RETRIES)
            {
                wifi_app_send_message(WIFI_APP_MSG_STA_DISCONNECTED);
                break;
            }

            wifi_event_sta_disconnected_t *wifi_event_sta_disconnected =
                (wifi_event_sta_disconnected_t *)malloc(sizeof(wifi_event_sta_disconnected_t));
            *wifi_event_sta_disconnected = *((wifi_event_sta_disconnected_t *)event_data);
            printf("WIFI_EVENT_AP_STA_DISCONNECTED, reason_code %d\n", wifi_event_sta_disconnected->reason);
            esp_wifi_connect();
            ++g_retry_number;
            break;

        default:
            break;
        }
    }
    else if (event_base == IP_EVENT)
    {
        switch (event_id)
        {
        case IP_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP");
            wifi_app_send_message(WIFI_APP_MSG_STA_CONNECTED_GOT_IP);
            break;
        default:
            break;
        }
    }
}

/**
 * @brief Initializes the WiFi app for the WiFi IP events
 *
 */
static void wifi_wifi_app_event_group_init()
{
    // Event loop for the WiFi driver
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Event handler for the connection
    esp_event_handler_instance_t instance_wifi_event;
    esp_event_handler_instance_t instance_ip_event;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_wifi_app_event_group, NULL,
                                                        &instance_wifi_event));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_wifi_app_event_group, NULL,
                                                        &instance_ip_event));
}

/**
 * @brief Initializes the TCP stack and default wifi configuration
 *
 */
static void wifi_app_default_wifi_init()
{
    // Initializes the TCP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Default wifi config
    // !Operations must be in this order!
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    esp_netif_sta = esp_netif_create_default_wifi_sta();
    esp_netif_ap = esp_netif_create_default_wifi_ap();
}

/**
 * @brief Configures the WiFi access point and assigns the static IP to the SoftAP
 *
 */
static void wifi_app_soft_ap_config()
{

    // SoftAP - WiFi access point configuration
    wifi_config_t ap_config = {
        .ap =
            {
                .ssid = WIFI_AP_SSID,
                .ssid_len = strlen(WIFI_AP_SSID),
                .password = WIFI_AP_PASSWORD,
                .channel = WIFI_AP_CHANNEL,
                .ssid_hidden = WIFI_AP_SSID_HIDDEN,
                .authmode = WIFI_AUTH_WPA2_PSK,
                .max_connection = WIFI_AP_MAX_CONNECTIONS,
                .beacon_interval = WIFI_AP_BEACON_INTERVAL,
            },
    };

    esp_netif_ip_info_t ap_ip_info;
    memset(&ap_ip_info, 0x00, sizeof(esp_netif_ip_info_t));

    // Must call this first before making any changes
    esp_netif_dhcps_stop(esp_netif_ap);
    // Assign access point's static IP, GW and netmask
    inet_pton(AF_INET, WIFI_AP_IP, &ap_ip_info.ip);
    inet_pton(AF_INET, WIFI_AP_GATEWAY, &ap_ip_info.gw);
    inet_pton(AF_INET, WIFI_AP_NETMASK, &ap_ip_info.netmask);

    // Statically configures network interface
    ESP_ERROR_CHECK(esp_netif_set_ip_info(esp_netif_ap, &ap_ip_info));
    // Start the AP (for connecting stations e.g. mobile devices)
    ESP_ERROR_CHECK(esp_netif_dhcps_start(esp_netif_ap));
    // Setting the mode as Access Point / Station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    // Set configuration
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));
    // Default Wifi bandwidth 20 Mhz
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(ESP_IF_WIFI_AP, WIFI_AP_BANDWIDTH));
    // Powersave set to NONE
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_STA_POWER_SAVE));
}

/**
 * @brief Connect the ESP32 to an external AP using the updated station configuration.
 */
static void wifi_app_connect_sta()
{
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_app_get_wifi_config()));
    ESP_ERROR_CHECK(esp_wifi_connect());
}

/**
 * @brief Main task for the WIFI application
 *
 * @param pvParameters parameter which can be passed to the task
 */
static void wifi_app_task(void *pvParameters)
{
    led_strip_handle_t led_strip = (led_strip_handle_t)pvParameters;

    wifi_app_queue_message_t msg;
    EventBits_t eventBits;

    wifi_wifi_app_event_group_init();

    wifi_app_default_wifi_init();

    wifi_app_soft_ap_config();

    // Start WIFI
    ESP_ERROR_CHECK(esp_wifi_start());

    // Send first event message
    wifi_app_send_message(WIFI_APP_MSG_STA_LOAD_SAVED_CREDENTIALS);

    while (true)
    {
        if (xQueueReceive(wifi_app_queue_handle, &msg, portMAX_DELAY))
        {
            switch (msg.messageID)
            {
            case WIFI_APP_MSG_STA_LOAD_SAVED_CREDENTIALS:
                ESP_LOGI(TAG, "WIFI_APP_MSG_STA_LOAD_SAVED_CREDENTIALS");
                if (app_nvs_load_sta_creds())
                {
                    ESP_LOGI(TAG, "Loading station configuration");
                    wifi_app_connect_sta();
                    xEventGroupSetBits(wifi_app_event_group, WIFI_APP_MSG_STA_LOAD_SAVED_CREDENTIALS_BIT);
                }
                else
                {
                    ESP_LOGI(TAG, "Unable to load station configuration");
                }
                wifi_app_send_message(WIFI_APP_MSG_START_HTTP_SERVER);

                break;

            case WIFI_APP_MSG_START_HTTP_SERVER:
                ESP_LOGI(TAG, "WIFI_APP_MSG_START_HTTP_SERVER");
                http_server_start();
                enable_light_color(led_strip, color_RED);
                break;

            case WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER:
                ESP_LOGI(TAG, "WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER");

                xEventGroupSetBits(wifi_app_event_group, WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER_BIT);

                enable_light_color(led_strip, color_BLUE);
                wifi_app_connect_sta();
                g_retry_number = 0;
                http_server_monitor_send_message(HTTP_MSG_WIFI_CONNECT_INIT);

                break;

            case WIFI_APP_MSG_STA_CONNECTED_GOT_IP:
                ESP_LOGI(TAG, "WIFI_APP_MSG_STA_CONNECTED_GOT_IP");
                enable_light_color(led_strip, color_WARM_WHITE);
                http_server_monitor_send_message(HTTP_MSG_WIFI_CONNECT_SUCCESS);

                eventBits = xEventGroupGetBits(wifi_app_event_group);
                /* Save credentials only when connecting from HTTP server */
                if (eventBits & WIFI_APP_MSG_STA_LOAD_SAVED_CREDENTIALS_BIT)
                {
                    /* Clear the bits in case we want to disconnect and reconnect */
                    xEventGroupClearBits(wifi_app_event_group, WIFI_APP_MSG_STA_LOAD_SAVED_CREDENTIALS_BIT);
                }
                else
                {
                    app_nvs_save_sta_creds();
                }
                if (eventBits & WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER_BIT)
                {
                    xEventGroupClearBits(wifi_app_event_group, WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER_BIT);
                }
                break;

            case WIFI_APP_MSG_STA_DISCONNECTED:
                ESP_LOGI(TAG, "WIFI_APP_MSG_STA_DISCONNECTED");

                eventBits = xEventGroupGetBits(wifi_app_event_group);
                if (eventBits & WIFI_APP_MSG_STA_LOAD_SAVED_CREDENTIALS_BIT)
                {
                    ESP_LOGI(TAG, "WIFI_APP_MSG_STA_DISCONNECTED: ATTEMPT LOG USING SAVED CREDENTIALS");
                    xEventGroupClearBits(wifi_app_event_group, WIFI_APP_MSG_STA_LOAD_SAVED_CREDENTIALS_BIT);
                    app_nvs_clear_sta_creds();
                }
                else if (eventBits & WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER_BIT)
                {
                    xEventGroupClearBits(wifi_app_event_group, WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER_BIT);
                    http_server_monitor_send_message(WIFI_REASON_CONNECTION_FAIL);
                }
                else if (eventBits & WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT_BIT)
                {
                    ESP_LOGI(TAG, "WIFI_APP_MSG_STA_DISCONNECTED: USER DISCONNECTION");
                    xEventGroupClearBits(wifi_app_event_group, WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT_BIT);
                    http_server_monitor_send_message(HTTP_MSG_WIFI_USER_DISCONNECT);
                }
                else
                {
                    ESP_LOGI(TAG, "WIFI_APP_MSG_STA_DISCONNECTED: ATTEMPT FAILED, CHECK WIFI AP ACCESSABILITY");
                    /* Adjust this case if u need to reconnect eg... */
                }

                break;

            case WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT:
                ESP_LOGI(TAG, "WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT");

                xEventGroupSetBits(wifi_app_event_group, WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT_BIT);
                g_retry_number = MAX_CONNECTIONS_RETRIES;
                ESP_ERROR_CHECK(esp_wifi_disconnect());
                break;

            default:
                disable_light(led_strip);
                break;
            }
        }
    }
}

BaseType_t wifi_app_send_message(wifi_app_message_e messageID)
{
    wifi_app_queue_message_t msg = {.messageID = messageID};
    return xQueueSend(wifi_app_queue_handle, &msg, portMAX_DELAY);
}

wifi_config_t *wifi_app_get_wifi_config()
{
    return wifi_config;
}

void wifi_app_start(led_strip_handle_t led_strip)
{
    ESP_LOGI(TAG, "Starting wifi application");
    esp_log_level_set("wifi", ESP_LOG_NONE);

    /* Allocate memory for the WiFi configuration */
    wifi_config = (wifi_config_t *)malloc(sizeof(wifi_config_t));
    memset(wifi_config, 0x00, sizeof(wifi_config_t));
    int32_t queue_length = 5;
    wifi_app_queue_handle = xQueueCreate(queue_length, sizeof(wifi_app_queue_message_t));

    wifi_app_event_group = xEventGroupCreate();
    xTaskCreatePinnedToCore(wifi_app_task, "wifi_app_task", WIFI_APP_TASK_STACK_SIZE, (void *)led_strip,
                            WIFI_APP_TASK_PRIORITY, NULL, WIFI_APP_TASK_CORE_ID);
}
