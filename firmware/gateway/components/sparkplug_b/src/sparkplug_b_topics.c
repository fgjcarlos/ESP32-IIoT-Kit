#include "sparkplug_b_topics.h"

#include <stdio.h>

static esp_err_t topic_format(char *out, size_t out_size, const char *format,
                              const char *group_id, const char *message_type,
                              const char *edge_node_id, const char *device_id)
{
    if (out == NULL || out_size == 0 || group_id == NULL || message_type == NULL ||
        edge_node_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    int written = device_id == NULL
        ? snprintf(out, out_size, format, group_id, message_type, edge_node_id)
        : snprintf(out, out_size, format, group_id, message_type, edge_node_id, device_id);
    if (written < 0 || (size_t)written >= out_size) return ESP_ERR_INVALID_SIZE;
    return ESP_OK;
}

esp_err_t sparkplug_b_topic_nbirth(const char *group_id, const char *edge_node_id,
                                    char *out, size_t out_size)
{
    return topic_format(out, out_size, "spBv1.0/%s/%s/%s", group_id, "NBIRTH",
                        edge_node_id, NULL);
}

esp_err_t sparkplug_b_topic_ndeath(const char *group_id, const char *edge_node_id,
                                    char *out, size_t out_size)
{
    return topic_format(out, out_size, "spBv1.0/%s/%s/%s", group_id, "NDEATH",
                        edge_node_id, NULL);
}

esp_err_t sparkplug_b_topic_ncmd(const char *group_id, const char *edge_node_id,
                                  char *out, size_t out_size)
{
    return topic_format(out, out_size, "spBv1.0/%s/%s/%s", group_id, "NCMD",
                        edge_node_id, NULL);
}

esp_err_t sparkplug_b_topic_dcmd(const char *group_id, const char *edge_node_id,
                                  const char *device_id, char *out, size_t out_size)
{
    if (device_id == NULL) return ESP_ERR_INVALID_ARG;
    return topic_format(out, out_size, "spBv1.0/%s/%s/%s/%s", group_id, "DCMD",
                        edge_node_id, device_id);
}

esp_err_t sparkplug_b_topic_ndata(const char *group_id, const char *edge_node_id,
                                  char *out, size_t out_size)
{
    return topic_format(out, out_size, "spBv1.0/%s/%s/%s", group_id, "NDATA",
                        edge_node_id, NULL);
}

esp_err_t sparkplug_b_topic_dbirth(const char *group_id, const char *edge_node_id,
                                   const char *device_id, char *out, size_t out_size)
{
    if (device_id == NULL) return ESP_ERR_INVALID_ARG;
    return topic_format(out, out_size, "spBv1.0/%s/%s/%s/%s", group_id, "DBIRTH",
                        edge_node_id, device_id);
}

esp_err_t sparkplug_b_topic_ddata(const char *group_id, const char *edge_node_id,
                                  const char *device_id, char *out, size_t out_size)
{
    if (device_id == NULL) return ESP_ERR_INVALID_ARG;
    return topic_format(out, out_size, "spBv1.0/%s/%s/%s/%s", group_id, "DDATA",
                        edge_node_id, device_id);
}

esp_err_t sparkplug_b_topic_ddeath(const char *group_id, const char *edge_node_id,
                                   const char *device_id, char *out, size_t out_size)
{
    if (device_id == NULL) return ESP_ERR_INVALID_ARG;
    return topic_format(out, out_size, "spBv1.0/%s/%s/%s/%s", group_id, "DDEATH",
                        edge_node_id, device_id);
}
