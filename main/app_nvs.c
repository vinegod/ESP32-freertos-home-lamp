#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"

#include "app_nvs.h"
#include "wifi_app.h"

/* Tag for logging monitor */
static const char *TAG = "nvs";

/* NVS namespace used for station mode credentials */
const char *app_nvs_sta_credentials_namespace = "sta_creds";

esp_err_t app_nvs_save_sta_creds()
{

    ESP_LOGI(TAG, "app_nvs_sta_credentials_namespace: Saving station mode credentials to flash");
    wifi_config_t *wifi_sta_config = wifi_app_get_wifi_config();
    if (wifi_sta_config == NULL)
    {
        return ESP_OK;
    }
    nvs_handle handle;
    esp_err_t esp_err = nvs_open(app_nvs_sta_credentials_namespace, NVS_READWRITE, &handle);
    if (esp_err != ESP_OK)
    {
        printf("app_nvs_save_sta_creds: Error (%s) opening nvs handle\n", esp_err_to_name(esp_err));
        return esp_err;
    }

    /* Set SSID */
    esp_err = nvs_set_blob(handle, "ssid", wifi_sta_config->sta.ssid, MAX_SSID_LENGTH);
    if (esp_err != ESP_OK)
    {
        printf("app_nvs_save_sta_creds: Error (%s) setting SSID to NVS\n", esp_err_to_name(esp_err));
        return esp_err;
    }
    /* Set password */
    esp_err = nvs_set_blob(handle, "password", wifi_sta_config->sta.password, MAX_PASSWORD_LENGTH);
    if (esp_err != ESP_OK)
    {
        printf("app_nvs_save_sta_creds: Error (%s) setting password to NVS\n", esp_err_to_name(esp_err));
        return esp_err;
    }

    /* Commit credentials to NVS */
    esp_err = nvs_commit(handle);
    if (esp_err != ESP_OK)
    {
        printf("app_nvs_save_sta_creds: Error (%s) committing credentials to NVS\n", esp_err_to_name(esp_err));
        return esp_err;
    }
    nvs_close(handle);
    printf("app_nvs_save_sta_creds: wrote wifi_sta_config: Station SSID: %s Password: %s\n", wifi_sta_config->sta.ssid,
           wifi_sta_config->sta.password);

    return ESP_OK;
}

bool app_nvs_load_sta_creds()
{
    /* TODO: malloc + memset -> calloc if its needed*/
    ESP_LOGI(TAG, "app_nvs_sta_credentials_namespace: Loading station mode credentials from flash");

    nvs_handle handle;
    if (nvs_open(app_nvs_sta_credentials_namespace, NVS_READONLY, &handle) != ESP_OK)
    {
        return false;
    }

    wifi_config_t *wifi_sta_config = wifi_app_get_wifi_config();
    size_t wifi_config_size = sizeof(wifi_config_t);
    if (wifi_sta_config == NULL)
    {
        wifi_sta_config = (wifi_config_t *)malloc(wifi_config_size);
    }
    memset(wifi_sta_config, 0x00, wifi_config_size);

    wifi_config_size = sizeof(wifi_sta_config->sta.ssid);
    esp_err_t esp_err = nvs_get_blob(handle, "ssid", &wifi_sta_config->sta.ssid, &wifi_config_size);
    if (esp_err != ESP_OK)
    {
        printf("app_nvs_load_sta_creds: Error (%s) no stations SSID found in NVS\n", esp_err_to_name(esp_err));
        return false;
    }

    wifi_config_size = sizeof(wifi_sta_config->sta.password);
    esp_err = nvs_get_blob(handle, "password", &wifi_sta_config->sta.password, &wifi_config_size);
    if (esp_err != ESP_OK)
    {
        printf("app_nvs_load_sta_creds: Error (%s) no stations password found in NVS\n", esp_err_to_name(esp_err));
        return false;
    }
    nvs_close(handle);
    printf("app_nvs_load_sta_creds: found wifi_sta_config: Station SSID: %s Password: %s\n", wifi_sta_config->sta.ssid,
           wifi_sta_config->sta.password);

    return wifi_sta_config->sta.ssid[0] != '\0';
}

esp_err_t app_nvs_clear_sta_creds()
{
    ESP_LOGI(TAG, "app_nvs_clear_sta_creds: Clearing station mode credentials from flash");

    nvs_handle handle;
    esp_err_t esp_err = nvs_open(app_nvs_sta_credentials_namespace, NVS_READONLY, &handle);
    if (esp_err != ESP_OK)
    {
        printf("app_nvs_clear_sta_creds: Error (%s) opening NVS handle\n", esp_err_to_name(esp_err));
        return esp_err;
    }

    /* Erase credentials */
    esp_err = nvs_erase_all(handle);
    if (esp_err != ESP_OK)
    {
        printf("app_nvs_clear_sta_creds: Error (%s) erasing station mode credentials\n", esp_err_to_name(esp_err));
        return esp_err;
    }

    /* Commit clearing credentials from NVS */
    esp_err = nvs_commit(handle);
    if (esp_err != ESP_OK)
    {
        printf("app_nvs_clear_sta_creds: Error (%s) NVS commit\n", esp_err_to_name(esp_err));
        return esp_err;
    }
    nvs_close(handle);
    printf("app_nvs_clear_sta_creds: Returned ESP_OK\n");

    return ESP_OK;
}