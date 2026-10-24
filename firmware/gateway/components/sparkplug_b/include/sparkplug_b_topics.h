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

/** @brief Topic for node data: spBv1.0/{group}/NDATA/{edge}. */
esp_err_t sparkplug_b_topic_ndata(const char *group_id, const char *edge_node_id, char *out, size_t out_size);

/** @brief Topic for a device birth: spBv1.0/{group}/DBIRTH/{edge}/{device}. */
esp_err_t sparkplug_b_topic_dbirth(const char *group_id, const char *edge_node_id, const char *device_id, char *out, size_t out_size);

/** @brief Topic for device data: spBv1.0/{group}/DDATA/{edge}/{device}. */
esp_err_t sparkplug_b_topic_ddata(const char *group_id, const char *edge_node_id, const char *device_id, char *out, size_t out_size);

/** @brief Topic for a device death: spBv1.0/{group}/DDEATH/{edge}/{device}. */
esp_err_t sparkplug_b_topic_ddeath(const char *group_id, const char *edge_node_id, const char *device_id, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif
