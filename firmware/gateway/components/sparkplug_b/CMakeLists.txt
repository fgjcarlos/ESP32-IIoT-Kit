# Sparkplug B component — commit 4.
#
# - Commit 1: stub + Kconfig + sdkconfig pin
# - Commit 3: header API stub (mqtt_bridge delegates here)
# - Commit 4: vendored .proto + nanopb-generated bindings + payload API
#             + minimal start/stop that logs an offline NBIRTH
# - Commit 5+: real MQTT client, state machine, NVS persistence
#
# Layout:
#   proto/sparkplug_b.proto    — vendored from eclipse/tahu 3.x (EPL-2.0)
#   proto/sparkplug_b.options  — nanopb sizing options (static arrays)
#   proto/sparkplug_b.pb.h     — nanopb-generated header (commit 4: vendored)
#   proto/sparkplug_b.pb.c     — nanopb-generated source  (commit 4: vendored)
#   proto/SOURCE.md            — upstream version + how to regenerate
#   src/sparkplug_b.c          — start/stop entry points (commit 4 logs only)
#   src/sparkplug_b_payload.c  — encode/decode helpers built on nanopb
#   include/sparkplug_b.h          — public API (start/stop)
#   include/sparkplug_b_payload.h  — public encode/decode API
#
# Why vendored .pb.{h,c}: ESP-IDF's component_get_requirements script-mode
# invocation rejects `add_custom_command`, which is the natural fit for
# `nanopb_generator.py`. Vendoring the generated bindings lets the build
# be reproducible offline and keeps CI simple. SOURCE.md documents the
# regeneration command for future contributors.

# We list the .pb.c directly in SRCS so the linker picks it up, and we
# add the proto directory to INCLUDE_DIRS so payload.c can find the
# generated header without polluting the public include surface.

idf_component_register(
    SRCS "src/sparkplug_b.c"
         "src/sparkplug_b_payload.c"
         "proto/sparkplug_b.pb.c"
    INCLUDE_DIRS "include" "proto"
    PRIV_REQUIRES "nanopb" "esp_timer")