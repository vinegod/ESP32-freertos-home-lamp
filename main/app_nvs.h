#ifndef APP_NVS_H_
#define APP_NVS_H_

#include <stdbool.h>

#include "esp_err.h"

/**
 * @brief Saves station mode Wi-Fi credentials to NVS.
 *
 * @return ESP_OK.
 */
esp_err_t app_nvs_save_sta_creds();

/**
 * @brief loads previous saved credentials from NVS
 *
 * @return true, if previous credentials were found, otherwise false.
 */
bool app_nvs_load_sta_creds();

/**
 * @brief Clears station mode credentials from NVS.
 *
 * @return ESP_OK
 */
esp_err_t app_nvs_clear_sta_creds();
#endif /* APP_NVS_H_ */
