/**
 * @file http_server.c
 * @brief The real local portal server over esp_http_server (W02, issue #69).
 *
 * Serves the embedded portal bundle (portal_assets) and the protected API:
 * login/logout with the HttpOnly session cookie of portal_auth, the status
 * document built from the wifi_manager getters, and the credential change.
 * The JSON bytes and the cookie parsing live in portal_api.c so the wire
 * contract is pinned by host and device tests.
 */

#include "http_server.h"

#include <string.h>

#include "brand_profile.h"
#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "portal_api.h"
#include "portal_assets.h"
#include "portal_auth.h"
#include "wifi_manager.h"

static const char *TAG = "http_server";

/** Request body cap: the largest valid API body (credential change) is
 * around 250 bytes, so anything bigger is rejected without reading it. */
#define PORTAL_BODY_MAX_LEN 512u

static httpd_handle_t s_server = NULL;

/* --- response helpers ---------------------------------------------------- */

static esp_err_t respond_error(httpd_req_t *req, const char *status,
                               const char *error_key)
{
    char body[64];
    if (portal_api_error_json(error_key, body, sizeof(body)) != ESP_OK) {
        return ESP_FAIL;
    }
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t respond_ok(httpd_req_t *req)
{
    httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    return httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
}

/** Read the request body into @p out (NUL-terminated, capped). */
static esp_err_t read_body(httpd_req_t *req, char out[PORTAL_BODY_MAX_LEN])
{
    const size_t len = req->content_len;
    if (len >= PORTAL_BODY_MAX_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (len > 0u) {
        const int recvd = httpd_req_recv(req, out, len);
        if (recvd < 0 || (size_t)recvd != len) {
            return ESP_FAIL;
        }
    }
    out[len] = '\0';
    return ESP_OK;
}

/** True (and @p token filled) when the request carries the active session. */
static bool session_token_of_request(httpd_req_t *req, char *token,
                                     size_t token_len)
{
    char cookie_header[192];
    const size_t len = httpd_req_get_hdr_value_len(req, "Cookie");
    if (len == 0u || len >= sizeof(cookie_header)) {
        return false;
    }
    if (httpd_req_get_hdr_value_str(req, "Cookie", cookie_header,
                                    sizeof(cookie_header)) != ESP_OK) {
        return false;
    }
    if (portal_api_cookie_value(cookie_header, token, token_len) != ESP_OK) {
        return false;
    }
    return portal_auth_session_valid(token);
}

/* --- URI handlers -------------------------------------------------------- */

/** Serve any embedded portal asset; the registered URI selects the entry. */
static esp_err_t portal_asset_handler(httpd_req_t *req)
{
    const portal_asset_t *asset = portal_assets_find(req->uri);
    if (asset == NULL) {
        /* Unreachable through the exact-URI registrations; kept so the
         * handler is safe on its own. */
        return respond_error(req, "404 Not Found", "no-encontrado");
    }
    httpd_resp_set_type(req, asset->mime);
    return httpd_resp_send(req, (const char *)asset->bytes, (ssize_t)asset->len);
}

static esp_err_t portal_login_handler(httpd_req_t *req)
{
    char body[PORTAL_BODY_MAX_LEN];
    if (read_body(req, body) != ESP_OK) {
        return respond_error(req, "400 Bad Request", "peticion-invalida");
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return respond_error(req, "400 Bad Request", "peticion-invalida");
    }
    const cJSON *credential = cJSON_GetObjectItemCaseSensitive(root, "credential");
    if (!cJSON_IsString(credential) || credential->valuestring == NULL) {
        cJSON_Delete(root);
        return respond_error(req, "400 Bad Request", "peticion-invalida");
    }

    char token[PORTAL_AUTH_TOKEN_STR_LEN];
    const esp_err_t err =
        portal_auth_login(credential->valuestring, token, sizeof(token));
    cJSON_Delete(root);
    if (err != ESP_OK) {
        if (err != ESP_ERR_NOT_FOUND && err != ESP_ERR_INVALID_SIZE) {
            ESP_LOGE(TAG, "portal_auth_login failed: %s", esp_err_to_name(err));
            return respond_error(req, "500 Internal Server Error", "interno");
        }
        /* Brute-force hardening (design D2): every rejected credential —
         * wrong or out of range alike — waits a fixed second before the
         * 401, so guessing is slowed without leaking which check failed. */
        vTaskDelay(pdMS_TO_TICKS(1000u));
        return respond_error(req, "401 Unauthorized", "credencial-incorrecta");
    }

    /* cookie value: name(14) + '=' + token(32) + "; Path=/; HttpOnly"(18)
     * + NUL = 66 bytes; the buffer leaves margin over that. */
    char cookie[PORTAL_AUTH_TOKEN_STR_LEN + 48];
    snprintf(cookie, sizeof(cookie),
             PORTAL_API_SESSION_COOKIE_NAME "=%s; Path=/; HttpOnly", token);
    httpd_resp_set_hdr(req, "Set-Cookie", cookie);
    return respond_ok(req);
}

static esp_err_t portal_status_handler(httpd_req_t *req)
{
    char token[PORTAL_AUTH_TOKEN_STR_LEN];
    if (!session_token_of_request(req, token, sizeof(token))) {
        return respond_error(req, "401 Unauthorized", "no-autorizado");
    }

    wifi_manager_status_t status;
    const esp_err_t err = wifi_manager_get_status(&status);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "wifi_manager_get_status failed: %s", esp_err_to_name(err));
        return respond_error(req, "503 Service Unavailable", "wifi-no-listo");
    }

    char body[PORTAL_API_STATUS_JSON_MAX_LEN];
    const uint64_t uptime_s = (uint64_t)(esp_timer_get_time() / 1000000LL);
    const esp_err_t json_err =
        portal_api_status_json(&status, uptime_s, portal_auth_is_initial_credential(),
                               body, sizeof(body));
    if (json_err != ESP_OK) {
        ESP_LOGE(TAG, "portal_api_status_json failed: %s",
                 esp_err_to_name(json_err));
        return respond_error(req, "500 Internal Server Error", "interno");
    }
    httpd_resp_set_type(req, HTTPD_TYPE_JSON);
    return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t portal_credential_handler(httpd_req_t *req)
{
    char token[PORTAL_AUTH_TOKEN_STR_LEN];
    if (!session_token_of_request(req, token, sizeof(token))) {
        return respond_error(req, "401 Unauthorized", "no-autorizado");
    }

    char body[PORTAL_BODY_MAX_LEN];
    if (read_body(req, body) != ESP_OK) {
        return respond_error(req, "400 Bad Request", "peticion-invalida");
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return respond_error(req, "400 Bad Request", "peticion-invalida");
    }
    const cJSON *current = cJSON_GetObjectItemCaseSensitive(root, "current");
    const cJSON *new_credential = cJSON_GetObjectItemCaseSensitive(root, "new");
    const cJSON *repeat = cJSON_GetObjectItemCaseSensitive(root, "repeat");
    if (!cJSON_IsString(current) || current->valuestring == NULL ||
        !cJSON_IsString(new_credential) || new_credential->valuestring == NULL ||
        !cJSON_IsString(repeat) || repeat->valuestring == NULL) {
        cJSON_Delete(root);
        return respond_error(req, "400 Bad Request", "peticion-invalida");
    }

    const esp_err_t err = portal_auth_change_credential(
        current->valuestring, new_credential->valuestring, repeat->valuestring);
    cJSON_Delete(root);
    switch (err) {
    case ESP_OK:
        return respond_ok(req);
    case ESP_ERR_NOT_FOUND:
        return respond_error(req, "404 Not Found", "credencial-incorrecta");
    case ESP_ERR_INVALID_ARG:
        return respond_error(req, "400 Bad Request", "repeticion-no-coincide");
    case ESP_ERR_INVALID_SIZE:
        return respond_error(req, "400 Bad Request", "longitud-fuera-de-rango");
    default:
        ESP_LOGE(TAG, "portal_auth_change_credential failed: %s",
                 esp_err_to_name(err));
        return respond_error(req, "500 Internal Server Error", "interno");
    }
}

static esp_err_t portal_logout_handler(httpd_req_t *req)
{
    char token[PORTAL_AUTH_TOKEN_STR_LEN];
    if (!session_token_of_request(req, token, sizeof(token))) {
        return respond_error(req, "401 Unauthorized", "no-autorizado");
    }

    const esp_err_t err = portal_auth_logout(token);
    if (err != ESP_OK) {
        if (err != ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "portal_auth_logout failed: %s", esp_err_to_name(err));
            return respond_error(req, "500 Internal Server Error", "interno");
        }
        return respond_error(req, "401 Unauthorized", "no-autorizado");
    }
    return respond_ok(req);
}

/* --- lifecycle ----------------------------------------------------------- */

static const httpd_uri_t k_portal_uris[] = {
    {.uri = "/", .method = HTTP_GET, .handler = portal_asset_handler},
    {.uri = "/app.js", .method = HTTP_GET, .handler = portal_asset_handler},
    {.uri = "/assets/index.css", .method = HTTP_GET, .handler = portal_asset_handler},
    {.uri = "/api/login", .method = HTTP_POST, .handler = portal_login_handler},
    {.uri = "/api/status", .method = HTTP_GET, .handler = portal_status_handler},
    {.uri = "/api/credential", .method = HTTP_POST, .handler = portal_credential_handler},
    {.uri = "/api/logout", .method = HTTP_POST, .handler = portal_logout_handler},
};

esp_err_t http_server_start(void)
{
    if (s_server != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const esp_err_t auth_err = portal_auth_init();
    if (auth_err != ESP_OK) {
        ESP_LOGE(TAG, "portal_auth_init failed: %s", esp_err_to_name(auth_err));
        return auth_err;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    /* AGENTS.md keeps tasks within the 4 KB default; the handlers only hold
     * the bounded status/body buffers (largest: 512 bytes). */
    config.stack_size = 4096;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        s_server = NULL;
        return err;
    }

    for (size_t i = 0; i < sizeof(k_portal_uris) / sizeof(k_portal_uris[0]); ++i) {
        err = httpd_register_uri_handler(s_server, &k_portal_uris[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "registering %s failed: %s", k_portal_uris[i].uri,
                     esp_err_to_name(err));
            (void)httpd_stop(s_server);
            s_server = NULL;
            return err;
        }
    }

    ESP_LOGI(TAG, "portal HTTP server listening on port %d", config.server_port);
    return ESP_OK;
}

esp_err_t http_server_stop(void)
{
    if (s_server == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const esp_err_t err = httpd_stop(s_server);
    s_server = NULL;
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_stop failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t http_server_brand_payload(char *out, size_t out_len)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Issue #12: the brand endpoint serves the identity compiled into the
     * firmware. The Fase 4 runtime registers the `/api/brand` URI handler
     * and hands it the bytes produced here; building the payload here keeps
     * the API, the LCD and the logs showing the same identity. */
    return brand_profile_format_json(brand_profile_get(), out, out_len);
}
