# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly


cmake_minimum_required(VERSION 3.16)
message(STATUS "Building: ${PROJECT}")

execute_process(COMMAND pip install jsmin csscompressor htmlmin zigpy)
execute_process(COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/compress.py ${CMAKE_CURRENT_SOURCE_DIR}/src_data ${CMAKE_CURRENT_SOURCE_DIR}/data)

set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/bluetooth/nimble/common/nimble_peripheral_utils)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(TICMeter)

idf_build_get_property(build_dir BUILD_DIR)
idf_build_get_property(elf_name EXECUTABLE_NAME GENERATOR_EXPRESSION)
set(app_bin "${build_dir}/${elf_name}.bin")
execute_process(COMMAND git describe --tags --always --dirty OUTPUT_VARIABLE GIT_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)

add_custom_command(OUTPUT /tmp/app.bin
DEPENDS "${build_dir}/.bin_timestamp"
COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/create-ota.py -m 65535 -i 200 -v ${GIT_VERSION} ${CMAKE_CURRENT_SOURCE_DIR}/build/TICMeter.bin ${CMAKE_CURRENT_SOURCE_DIR}/build/storage.bin ${CMAKE_CURRENT_SOURCE_DIR}/build/TICMeter.ota
COMMENT "Creating OTA file"
)

add_custom_target(copy_app_binary ALL DEPENDS /tmp/app.bin)
add_dependencies(copy_app_binary gen_project_binary)