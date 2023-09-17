set(TUYALINK_DIR ${COMPONENT_DIR}/tuya-connect-kit-for-mqtt-embedded-c)

include( ${TUYALINK_DIR}/tuyaFilePaths.cmake )
include( ${TUYALINK_DIR}/libraries/coreJSON/jsonFilePaths.cmake )
include( ${TUYALINK_DIR}/libraries/coreHTTP/httpFilePaths.cmake )
include( ${TUYALINK_DIR}/libraries/coreMQTT/mqttFilePaths.cmake )

FILE(GLOB UTILS_SOURCES ${TUYALINK_DIR}/utils/*.c)

set(include_dirs
    ${LINK_SDK_INCLUDE_PUBLIC_DIRS}
    ${JSON_INCLUDE_PUBLIC_DIRS}
    ${HTTP_INCLUDE_PUBLIC_DIRS}
    ${MQTT_INCLUDE_PUBLIC_DIRS}
    ${TUYALINK_DIR}/utils
    ${TUYALINK_DIR}/interface
    ${TUYALINK_DIR}/middleware )

set(srcs
    "port/system_wrapper.c"
    "port/storage_wrapper.c"
    "port/network_wrapper.c"
    ${TUYALINK_DIR}/middleware/http_client_wrapper.c
    ${TUYALINK_DIR}/middleware/mqtt_client_wrapper.c
    ${UTILS_SOURCES}
    ${LINK_SDK_SOURCES}
    ${JSON_SOURCES}
    ${HTTP_SOURCES}
    ${MQTT_SOURCES}
    ${MQTT_SERIALIZER_SOURCES})

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "${include_dirs}"
                    REQUIRES lwip mbedtls nvs_flash esp_event esp_netif)

# Silence format truncation warning, until it is fixed upstream
# set_source_files_properties(port/coap_debug.c PROPERTIES COMPILE_FLAGS -Wno-format-truncation)

# Needed for coap headers in public builds, also.
#
# TODO: find a way to move this to a port header
target_compile_definitions(${COMPONENT_LIB} PUBLIC WITH_POSIX)
