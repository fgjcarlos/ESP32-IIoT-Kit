#include "sparkplug_b_state.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static sparkplug_b_state_t s_state;
static StaticSemaphore_t s_mutex_storage;
static SemaphoreHandle_t s_mutex = NULL;

static SemaphoreHandle_t state_mutex(void)
{
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutexStatic(&s_mutex_storage);
    }
    return s_mutex;
}

void sparkplug_b_state_reset(void)
{
    SemaphoreHandle_t mutex = state_mutex();
    if (mutex != NULL && xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        s_state = (sparkplug_b_state_t){0};
        xSemaphoreGive(mutex);
    }
}

sparkplug_b_state_t *sparkplug_b_state_get(void)
{
    return &s_state;
}

esp_err_t sparkplug_b_state_set(const sparkplug_b_state_t *state)
{
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    SemaphoreHandle_t mutex = state_mutex();
    if (mutex == NULL || xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    s_state = *state;
    xSemaphoreGive(mutex);
    return ESP_OK;
}

uint64_t sparkplug_b_seq_next(void)
{
    SemaphoreHandle_t mutex = state_mutex();
    if (mutex == NULL || xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return 0;
    }
    s_state.seq = (s_state.seq >= UINT8_MAX) ? 0 : s_state.seq + 1;
    uint64_t next = s_state.seq;
    xSemaphoreGive(mutex);
    return next;
}

esp_err_t sparkplug_b_bd_seq_save(nvs_handle_t handle, uint64_t bd_seq)
{
    esp_err_t err = nvs_set_u64(handle, "mqtt_bd_seq", bd_seq);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    if (err == ESP_OK) {
        sparkplug_b_state_t state = *sparkplug_b_state_get();
        state.bd_seq = bd_seq;
        err = sparkplug_b_state_set(&state);
    }
    return err;
}

esp_err_t sparkplug_b_bd_seq_load(nvs_handle_t handle, uint64_t *bd_seq)
{
    if (bd_seq == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = nvs_get_u64(handle, "mqtt_bd_seq", bd_seq);
    if (err == ESP_OK) {
        sparkplug_b_state_t state = *sparkplug_b_state_get();
        state.bd_seq = *bd_seq;
        err = sparkplug_b_state_set(&state);
    }
    return err;
}
