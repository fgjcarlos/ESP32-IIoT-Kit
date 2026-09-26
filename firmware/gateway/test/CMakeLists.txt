cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "../components")
set(TEST_COMPONENTS "board_profile;board_rgb" CACHE STRING "List of components to test")

list(FIND TEST_COMPONENTS "board_rgb" board_rgb_test_component_index)
if(board_rgb_test_component_index EQUAL -1)
    list(APPEND TEST_COMPONENTS "board_rgb")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(gateway_test)
