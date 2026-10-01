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
uint64_t sparkplug_b_seq_next(void);
esp_err_t sparkplug_b_bd_seq_save(nvs_handle_t handle, uint64_t bd_seq);
esp_err_t sparkplug_b_bd_seq_load(nvs_handle_t handle, uint64_t *bd_seq);

#ifdef __cplusplus
}
#endif
