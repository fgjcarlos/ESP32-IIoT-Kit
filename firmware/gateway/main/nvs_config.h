#pragma once

#include "esp_err.h"

/**
 * @brief Initialize the gateway NVS partition.
 *
 * NVS namespace ownership table:
 * - "wifi": Wi-Fi configuration.
 * - "mqtt": MQTT configuration.
 * - "ota": OTA configuration.
 * - "display": display configuration.
 * - "node": sensor-node configuration.
 *
 * The documented default key is `mqtt_namespace = "iiot-kit"` (source:
 * `docs/replanificacion/02-protocolo-unificado.md`). Typed accessors and
 * namespace operations are outside this bootstrap interface.
 *
 * @return ESP_OK on successful initialization, otherwise the ESP-IDF error.
 */
esp_err_t nvs_config_init(void);
