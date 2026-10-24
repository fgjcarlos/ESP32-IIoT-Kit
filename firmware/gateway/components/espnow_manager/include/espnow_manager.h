#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define ESPNOW_MANAGER_MAX_PEERS 20U

/**
 * @brief Initialize the ESP-NOW manager contract.
 *
 * Interface only — full implementation owned by the Fase 1 T1.4.x change.
 *
 * @return ESP_OK for the contract-only placeholder.
 */
esp_err_t espnow_manager_init(void);

/**
 * @brief Hand a serialized ESP-NOW frame to the radio.
 *
 * The radio path is not implemented yet (Fase 1, T1.4.x owns the peer table,
 * the channel lock and the send loop), so the contract logs the frame and
 * reports success.
 *
 * The signature exists now because the Sparkplug command path needs it: a
 * DCMD that asks a node to reboot or to change its sleep interval is
 * translated into an ESP-NOW config frame, and the gateway has to be able to
 * hand that frame to the radio without the Sparkplug module knowing anything
 * about the ESP-NOW stack.
 *
 * @param frame Serialized ESP-NOW frame.
 * @param len   Frame length in bytes.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if the frame is missing.
 */
esp_err_t espnow_manager_send(const uint8_t *frame, size_t len);
