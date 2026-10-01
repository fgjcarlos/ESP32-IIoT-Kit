#pragma once

#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t sparkplug_b_topic_nbirth(const char *group_id, const char *edge_node_id, char *out, size_t out_size);
esp_err_t sparkplug_b_topic_ndeath(const char *group_id, const char *edge_node_id, char *out, size_t out_size);
esp_err_t sparkplug_b_topic_ncmd(const char *group_id, const char *edge_node_id, char *out, size_t out_size);
esp_err_t sparkplug_b_topic_dcmd(const char *group_id, const char *edge_node_id, const char *device_id, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif
