#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "nvs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t seq;
    uint64_t bd_seq;
} sparkplug_b_state_t;

void sparkplug_b_state_reset(void);
sparkplug_b_state_t *sparkplug_b_state_get(void);
esp_err_t sparkplug_b_state_set(const sparkplug_b_state_t *state);

/**
 * @brief Take the next sequence number for a Sparkplug message.
 *
 * Sparkplug B v3.0.0 §6.4.1: the sequence number starts at 0 for the NBIRTH
 * and increments by one per message, wrapping from 255 back to 0.
 */
uint64_t sparkplug_b_seq_next(void);

/**
 * @brief Reset the sequence number to 0 and return it.
 *
 * Called right before an NBIRTH: the birth always opens the sequence of the
 * session, so it carries seq 0 and the next message carries seq 1.
 */
uint64_t sparkplug_b_seq_reset(void);

/**
 * @brief Increment the birth/death sequence number and return the new value.
 *
 * Sparkplug B v3.0.0 §6.4.1: bdSeq is a single byte that increments on every
 * new MQTT CONNECT and wraps from 255 back to 0. It must survive a reboot so
 * a late NDEATH can be matched against the NBIRTH that opened the session.
 */
uint64_t sparkplug_b_bd_seq_next(void);

esp_err_t sparkplug_b_bd_seq_save(nvs_handle_t handle, uint64_t bd_seq);
esp_err_t sparkplug_b_bd_seq_load(nvs_handle_t handle, uint64_t *bd_seq);

#ifdef __cplusplus
}
#endif
