cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "../components")
set(TEST_COMPONENTS "board_profile;board_rgb" CACHE STRING "List of components to test")

list(FIND TEST_COMPONENTS "board_rgb" board_rgb_test_component_index)
if(board_rgb_test_component_index EQUAL -1)
    list(APPEND TEST_COMPONENTS "board_rgb")
endif()

list(FIND TEST_COMPONENTS "espnow_manager" espnow_manager_test_component_index)
if(espnow_manager_test_component_index EQUAL -1)
    list(APPEND TEST_COMPONENTS "espnow_manager")
endif()

list(FIND TEST_COMPONENTS "http_server" http_server_test_component_index)
if(http_server_test_component_index EQUAL -1)
    list(APPEND TEST_COMPONENTS "http_server")
endif()

list(FIND TEST_COMPONENTS "mqtt_bridge" mqtt_bridge_test_component_index)
if(mqtt_bridge_test_component_index EQUAL -1)
    list(APPEND TEST_COMPONENTS "mqtt_bridge")
endif()

list(FIND TEST_COMPONENTS "ota_manager" ota_manager_test_component_index)
if(ota_manager_test_component_index EQUAL -1)
    list(APPEND TEST_COMPONENTS "ota_manager")
endif()

list(FIND TEST_COMPONENTS "display_manager" display_manager_test_component_index)
if(display_manager_test_component_index EQUAL -1)
    list(APPEND TEST_COMPONENTS "display_manager")
endif()

list(FIND TEST_COMPONENTS "lcd_driver" lcd_driver_test_component_index)
if(lcd_driver_test_component_index EQUAL -1)
    list(APPEND TEST_COMPONENTS "lcd_driver")
endif()

list(FIND TEST_COMPONENTS "lvgl_port" lvgl_port_test_component_index)
if(lvgl_port_test_component_index EQUAL -1)
    list(APPEND TEST_COMPONENTS "lvgl_port")
endif()

list(FIND TEST_COMPONENTS "ui_screens" ui_screens_test_component_index)
if(ui_screens_test_component_index EQUAL -1)
    list(APPEND TEST_COMPONENTS "ui_screens")
endif()

list(FIND TEST_COMPONENTS "assets_brand" assets_brand_test_component_index)
if(assets_brand_test_component_index EQUAL -1)
    list(APPEND TEST_COMPONENTS "assets_brand")
endif()

list(FIND TEST_COMPONENTS "wifi_manager" wifi_manager_test_component_index)
if(wifi_manager_test_component_index EQUAL -1)
    list(APPEND TEST_COMPONENTS "wifi_manager")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(gateway_test)
