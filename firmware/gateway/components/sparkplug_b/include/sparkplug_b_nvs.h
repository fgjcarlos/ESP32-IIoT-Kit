/**
 * @file sparkplug_b_nvs.h
 * @brief NVS namespace and keys used by the Sparkplug B component.
 *
 * Why this file exists
 * --------------------
 * NVS rejects a key longer than `NVS_MAX_KEY_LEN` characters with
 * `ESP_ERR_NVS_KEY_TOO_LONG`. The first implementation of this component used
 * descriptive key names such as `mqtt_edge_node_id` (18 characters) and
 * `mqtt_ddeath_timeout_sec` (23), so the whole configuration load failed on
 * the target with `ESP_ERR_NVS_KEY_TOO_LONG` and the gateway never opened its
 * MQTT session. The host test runner did not catch it because it does not
 * reproduce the NVS key length limit.
 *
 * Every key of the namespace therefore lives here, is named after the value
 * it holds, and is listed in `SPARKPLUG_B_NVS_KEYS()`. The regression test in
 * `test/test_nvs.c` walks that list and fails when a key is too long, so a
 * new key cannot silently reintroduce the defect.
 *
 * The namespace is `mqtt` (one namespace per module, as the project
 * conventions require) and it holds the configuration of the MQTT transport:
 * the broker URI, the credentials, the Sparkplug identity and the session
 * parameters.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Maximum length of an NVS key, in characters.
 *
 * It mirrors `NVS_MAX_KEY_LEN` of the ESP-IDF `nvs_flash` component. The
 * value is repeated here on purpose: the host test runner does not link
 * `nvs_flash`, so the constraint has to be expressible without it.
 */
#define SPARKPLUG_B_NVS_KEY_MAX_LEN 15

/** NVS namespace that holds the MQTT and Sparkplug configuration. */
#define SPARKPLUG_B_NVS_NAMESPACE "mqtt"

/** Broker URI, for example `mqtt://192.168.1.10:1883`. */
#define SPARKPLUG_B_NVS_KEY_URI "mqtt_uri"

/** User name of the broker, empty when the broker has no authentication. */
#define SPARKPLUG_B_NVS_KEY_USER "mqtt_user"

/** Password of the broker, empty when the broker has no authentication. */
#define SPARKPLUG_B_NVS_KEY_PASSWORD "mqtt_password"

/** TLS flag: non-zero when the broker connection uses TLS. */
#define SPARKPLUG_B_NVS_KEY_TLS "mqtt_tls"

/** Sparkplug group id, `iiot-kit` by default. */
#define SPARKPLUG_B_NVS_KEY_GROUP_ID "mqtt_group_id"

/** Sparkplug edge node id, derived from the MAC when the key is absent. */
#define SPARKPLUG_B_NVS_KEY_EDGE_ID "mqtt_edge_id"

/** MQTT keepalive in seconds, 60 by default. */
#define SPARKPLUG_B_NVS_KEY_KEEPALIVE "mqtt_keepalive"

/** DDEATH expiry timeout per device in seconds, 1800 by default. */
#define SPARKPLUG_B_NVS_KEY_DDEATH_SEC "mqtt_ddeath_sec"

/** Birth/death sequence number of the last session. */
#define SPARKPLUG_B_NVS_KEY_BD_SEQ "mqtt_bd_seq"

/**
 * Every NVS key of the namespace, as an argument list.
 *
 * `X` is expanded with one argument per key, so the list is the single place
 * where a new key has to be declared:
 *
 *     #define CHECK(key) TEST_ASSERT_LESS_OR_EQUAL(SPARKPLUG_B_NVS_KEY_MAX_LEN, strlen(key));
 *     SPARKPLUG_B_NVS_KEYS(CHECK)
 */
#define SPARKPLUG_B_NVS_KEYS(X)  \
    X(SPARKPLUG_B_NVS_KEY_URI)    \
    X(SPARKPLUG_B_NVS_KEY_USER)   \
    X(SPARKPLUG_B_NVS_KEY_PASSWORD) \
    X(SPARKPLUG_B_NVS_KEY_TLS)    \
    X(SPARKPLUG_B_NVS_KEY_GROUP_ID) \
    X(SPARKPLUG_B_NVS_KEY_EDGE_ID)  \
    X(SPARKPLUG_B_NVS_KEY_KEEPALIVE) \
    X(SPARKPLUG_B_NVS_KEY_DDEATH_SEC) \
    X(SPARKPLUG_B_NVS_KEY_BD_SEQ)

#ifdef __cplusplus
}
#endif
