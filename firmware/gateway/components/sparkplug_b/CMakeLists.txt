idf_component_register(
    SRCS "src/sparkplug_b.c"
         "src/sparkplug_b_mqtt.c"
         "src/sparkplug_b_state.c"
         "src/sparkplug_b_topics.c"
         "src/sparkplug_b_payload.c"
         "proto/sparkplug_b.pb.c"
    INCLUDE_DIRS "include" "proto"
    REQUIRES esp_common log mqtt esp_event esp-tls mbedtls nvs_flash esp_timer
    PRIV_REQUIRES nanopb)
