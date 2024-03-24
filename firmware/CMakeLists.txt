# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly


cmake_minimum_required(VERSION 3.16)
message(STATUS "Building: ${PROJECT}")
execute_process(COMMAND pip install jsmin csscompressor htmlmin)
execute_process(COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/compress.py ${CMAKE_CURRENT_SOURCE_DIR}/src_data ${CMAKE_CURRENT_SOURCE_DIR}/data)
set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/bluetooth/nimble/common/nimble_peripheral_utils)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(TICMeter)
