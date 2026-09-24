cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "../components")
set(TEST_COMPONENTS "board_profile" CACHE STRING "List of components to test")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(gateway_test)
