/**
 * @file portal_api.c
 * @brief Portable JSON/cookie helpers of the local portal API (W02, issue #69).
 *
 * The exact response bytes of the portal API live here, away from the
 * esp_http_server glue, so the host and device Unity tests pin the wire
 * contract (compact JSON, stable error keys, HttpOnly cookie name).
 */

#include "portal_api.h"

#include <stdio.h>
#include <string.h>

/** Cookie-name prefix the parser looks for, including the '='. */
#define PORTAL_API_COOKIE_PREFIX PORTAL_API_SESSION_COOKIE_NAME "="

/**
 * @brief Append-only writer over the response buffer.
 *
 * The status document interpolates strings that come from the device
 * configuration (an SSID may legally hold quotes or control characters),
 * so they are escaped instead of copied raw into the JSON.
 */
typedef struct {
    char *buf;
    size_t len;
    size_t pos;
} json_writer_t;

static void json_writer_init(json_writer_t *w, char *buf, size_t len)
{
    w->buf = buf;
    w->len = len;
    w->pos = 0u;
    if (len > 0u) {
        buf[0] = '\0';
    }
}

static bool json_writer_raw(json_writer_t *w, const char *text)
{
    const size_t n = strlen(text);
    if ((w->pos + n) >= w->len) {
        return false;
    }
    memcpy(w->buf + w->pos, text, n);
    w->pos += n;
    w->buf[w->pos] = '\0';
    return true;
}

static bool json_writer_escaped(json_writer_t *w, const char *src)
{
    if (src == NULL) {
        return json_writer_raw(w, "");
    }
    for (const char *p = src; *p != '\0'; ++p) {
        char escaped[8] = {0};
        size_t n = 1u;
        if (*p == '"') {
            escaped[0] = '\\';
            escaped[1] = '"';
            n = 2u;
        } else if (*p == '\\') {
            escaped[0] = '\\';
            escaped[1] = '\\';
            n = 2u;
        } else if ((unsigned char)*p < 0x20u) {
            n = (size_t)snprintf(escaped, sizeof(escaped), "\\u%04x",
                                  (unsigned char)*p);
        } else {
            escaped[0] = *p;
        }
        if ((w->pos + n) >= w->len) {
            return false;
        }
        memcpy(w->buf + w->pos, escaped, n);
        w->pos += n;
    }
    w->buf[w->pos] = '\0';
    return true;
}

static bool json_writer_u64(json_writer_t *w, uint64_t value)
{
    char text[24];
    const int written = snprintf(text, sizeof(text), "%llu",
                                  (unsigned long long)value);
    return (written > 0) && json_writer_raw(w, text);
}

static bool json_writer_u8(json_writer_t *w, uint8_t value)
{
    char text[8];
    const int written = snprintf(text, sizeof(text), "%u", (unsigned int)value);
    return (written > 0) && json_writer_raw(w, text);
}

static const char *sta_state_name(wifi_manager_sta_state_t state)
{
    switch (state) {
    case WIFI_MANAGER_STA_IDLE:
        return "idle";
    case WIFI_MANAGER_STA_CONNECTING:
        return "connecting";
    case WIFI_MANAGER_STA_CONNECTED:
        return "connected";
    case WIFI_MANAGER_STA_DISCONNECTED:
        return "disconnected";
    default:
        return NULL;
    }
}

esp_err_t portal_api_status_json(const wifi_manager_status_t *status,
                                 uint64_t uptime_s, bool initial_credential,
                                 char *out, size_t out_len)
{
    if (status == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const char *sta_state = sta_state_name(status->sta_state);
    if (sta_state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    json_writer_t w;
    json_writer_init(&w, out, out_len);

    const bool ok =
        json_writer_raw(&w, "{\"ap\":{\"up\":") &&
        json_writer_raw(&w, status->ap_up ? "true" : "false") &&
        json_writer_raw(&w, ",\"ssid\":\"") &&
        json_writer_escaped(&w, status->ap_ssid) &&
        json_writer_raw(&w, "\",\"ip\":\"") &&
        json_writer_escaped(&w, status->ap_ip) &&
        json_writer_raw(&w, "\",\"channel\":") &&
        json_writer_u8(&w, status->ap_channel) &&
        json_writer_raw(&w, ",\"mac\":\"") &&
        json_writer_escaped(&w, status->ap_mac) &&
        json_writer_raw(&w, "\"},\"sta\":{\"state\":\"") &&
        json_writer_raw(&w, sta_state) &&
        json_writer_raw(&w, "\",\"ssid\":\"") &&
        json_writer_escaped(&w, status->sta_ssid) &&
        json_writer_raw(&w, "\",\"ip\":\"") &&
        json_writer_escaped(&w, status->sta_ip) &&
        json_writer_raw(&w, "\"},\"uptime_s\":") &&
        json_writer_u64(&w, uptime_s) &&
        json_writer_raw(&w, ",\"portal\":{\"initial_credential\":") &&
        json_writer_raw(&w, initial_credential ? "true" : "false") &&
        json_writer_raw(&w, "}}");

    if (!ok) {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

esp_err_t portal_api_error_json(const char *error_key, char *out, size_t out_len)
{
    if (error_key == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const int needed = snprintf(out, out_len, "{\"error\":\"%s\"}", error_key);
    if (needed < 0) {
        return ESP_FAIL;
    }
    if ((size_t)needed >= out_len) {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

esp_err_t portal_api_cookie_value(const char *cookie_header, char *out,
                                  size_t out_len)
{
    if (cookie_header == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const size_t prefix_len = strlen(PORTAL_API_COOKIE_PREFIX);
    const char *pair = cookie_header;
    while (*pair != '\0') {
        while (*pair == ' ') {
            ++pair;
        }
        if (strncmp(pair, PORTAL_API_COOKIE_PREFIX, prefix_len) == 0) {
            const char *value = pair + prefix_len;
            const char *end = strchr(value, ';');
            const size_t value_len =
                (end != NULL) ? (size_t)(end - value) : strlen(value);
            if (value_len + 1u > out_len) {
                return ESP_ERR_INVALID_SIZE;
            }
            memcpy(out, value, value_len);
            out[value_len] = '\0';
            return ESP_OK;
        }
        const char *next = strchr(pair, ';');
        if (next == NULL) {
            break;
        }
        pair = next + 1;
    }
    return ESP_ERR_NOT_FOUND;
}
