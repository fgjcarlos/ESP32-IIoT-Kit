# Sparkplug B component (stub for commit 1)
# Real sources, idf_component.yml, .proto and tests land in commits 3–4.
# This stub exists so that CONFIG_SPARKPLUG_B_ENABLED appears in `idf.py menuconfig`
# and so that sdkconfig.defaults can pin the Kconfig symbol to "not set" by default.

idf_component_register(SRCS ""
                    INCLUDE_DIRS ""
                    REQUIRES "")