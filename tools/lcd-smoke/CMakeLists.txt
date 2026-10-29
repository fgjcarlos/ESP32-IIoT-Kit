# Standalone ESP-IDF project: the ST7789 image diagnostic.
#
# It is a project of its own on purpose. The diagnostic has to be flashable
# without the gateway firmware, and it must not reach into the shared
# components of the repository.
#
# The panel is wired to an ESP32-S3-WROOM-1-N16R8 carrier, so the target is
# pinned here and the documented workflow does not need `idf.py set-target`.

cmake_minimum_required(VERSION 3.16)

set(IDF_TARGET "esp32s3")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(lcd_smoke)
