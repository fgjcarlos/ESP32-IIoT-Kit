#pragma once

#include "esp_err.h"

/**
 * @brief Start the MQTT bridge.
 *
 * When CONFIG_SPARKPLUG_B_ENABLED is enabled, this facade delegates to
 * sparkplug_b. Otherwise, it remains an interface-only no-op placeholder.
 *
 * @return ESP_OK when the bridge starts or remains a disabled no-op.
 */
esp_err_t mqtt_bridge_start(void);

/**
 * @brief Stop the MQTT bridge.
 *
 * When CONFIG_SPARKPLUG_B_ENABLED is enabled, this facade delegates to
 * sparkplug_b. Otherwise, it remains an interface-only no-op placeholder.
 *
 * @return ESP_OK when the bridge stops or remains a disabled no-op.
 */
esp_err_t mqtt_bridge_stop(void);
