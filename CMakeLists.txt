# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

###############################################################################
# Validate that we are on a supported version of ESP-IDF (v4.3+)
###############################################################################
if (IDF_VERSION_MAJOR LESS 4)
    message(FATAL_ERROR "ESP32CommandStation requires IDF v4.0 (or newer)")
endif()

if (IDF_VERSION_MAJOR EQUAL 4 AND IDF_VERSION_MINOR LESS 3)
    message(FATAL_ERROR "ESP32 Command Station requires IDF v4.3 (or newer)")
endif()

###############################################################################
# Verify submodules are up-to-date
###############################################################################

git_submodule_check(${CMAKE_CURRENT_SOURCE_DIR})

###############################################################################
# Disable ESP-IDF components that are not required for this project. This will
# reduce the compile time.
###############################################################################

set(EXCLUDE_COMPONENTS
    "asio" "bt" "cmock" "coap" "esp-tls" "esp_hid"
    "esp_http_client" "esp_http_server" "esp_https_ota" "esp_https_server"
    "esp_lcd" "esp_local_ctrl" "esp_serial_slave_link" "esp_websocket_client"
    "freemodbus" "idf_test" "mqtt" "nghttp" "perfmon" "protobuf-c" "protocomm"
    "tcp_transport" "tinyusb" "unity" "wifi_provisioning")

project(ESP32CommandStation)

###############################################################################
# Switch from GNU++11 to GNU++14
#
# NOTE: C++14 would be preferred but IDF has dependencies on GNU extensions.
###############################################################################

string(REPLACE "-std=gnu++11" "-std=gnu++14" CXX_OPTIONS "${CXX_COMPILE_OPTIONS}")
idf_build_set_property(CXX_COMPILE_OPTIONS "${CXX_OPTIONS}" REPLACE)

###############################################################################
# Enable usage of std::stoi/stol/etc
###############################################################################

idf_build_set_property(COMPILE_DEFINITIONS "-D_GLIBCXX_USE_C99" APPEND)

###############################################################################
# Silence warnings due to OpenMRN code.
###############################################################################

idf_build_set_property(COMPILE_DEFINITIONS "-Wno-ignored-qualifiers" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS "-Wno-implicit-fallthrough" APPEND)

###############################################################################
# Configure SNIP parameters
###############################################################################

# export the project version as a define for the SNIP data, note it must be
# truncated to 21 characters max.
idf_build_get_property(project_ver PROJECT_VER)
string(SUBSTRING "${project_ver}" 0 20 SNIP_SW_VERSION)
idf_build_get_property(project_name PROJECT_NAME)
string(SUBSTRING "${project_name}" 0 40 SNIP_PROJECT_NAME)

set(SNIP_HW_VERSION "1.0.0")
set(SNIP_PROJECT_PAGE "atanisoft.github.io")
set(CDI_VERSION "0x0200")
message(STATUS "SNIP: ${SNIP_PROJECT_NAME}/${SNIP_PROJECT_PAGE}/${SNIP_SW_VERSION}/${SNIP_HW_VERSION}")

idf_build_set_property(COMPILE_DEFINITIONS "-DSNIP_PROJECT_PAGE=\"${SNIP_PROJECT_PAGE}\"" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS "-DSNIP_HW_VERSION=\"${SNIP_HW_VERSION}\"" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS "-DSNIP_SW_VERSION=\"${SNIP_SW_VERSION}\"" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS "-DSNIP_PROJECT_NAME=\"${SNIP_PROJECT_NAME}\"" APPEND)
idf_build_set_property(COMPILE_DEFINITIONS "-DCDI_VERSION=${CDI_VERSION}" APPEND)

###############################################################################
# Search for GZIP application
###############################################################################

FIND_PROGRAM(GZIP
  NAMES gzip
  PATHS /bin
        /usr/bin
        /usr/local/bin
)

if (NOT GZIP)
  message(FATAL_ERROR "Unable to find 'gzip' program")
endif()

###############################################################################
# Generate a compressed version of web content on-demand
###############################################################################

add_custom_command(OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/web/index.html.gz"
    COMMAND ${GZIP} -fk ${CMAKE_CURRENT_SOURCE_DIR}/web/index.html
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/web/index.html
    VERBATIM)
set_property(TARGET ${CMAKE_PROJECT_NAME}.elf APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/web/index.html.gz")

add_custom_command(OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/web/cash.min.js.gz"
    COMMAND ${GZIP} -fk ${CMAKE_CURRENT_SOURCE_DIR}/web/cash.min.js
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/web/cash.min.js
    VERBATIM)
set_property(TARGET ${CMAKE_PROJECT_NAME}.elf APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/web/cash.min.js.gz")

add_custom_command(OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/web/spectre.min.css.gz"
    COMMAND ${GZIP} -fk ${CMAKE_CURRENT_SOURCE_DIR}/web/spectre.min.css
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/web/spectre.min.css
    VERBATIM)
set_property(TARGET ${CMAKE_PROJECT_NAME}.elf APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/web/spectre.min.css.gz")

add_custom_command(OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/web/cdi.js.gz"
    COMMAND ${GZIP} -fk ${CMAKE_CURRENT_SOURCE_DIR}/web/cdi.js
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/web/cdi.js
    VERBATIM)
set_property(TARGET ${CMAKE_PROJECT_NAME}.elf APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/web/cdi.js.gz")
###############################################################################
# Add web content to the binary
###############################################################################

target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "${CMAKE_CURRENT_SOURCE_DIR}/web/index.html.gz" BINARY)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "${CMAKE_CURRENT_SOURCE_DIR}/web/cash.min.js.gz" BINARY)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "${CMAKE_CURRENT_SOURCE_DIR}/web/spectre.min.css.gz" BINARY)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "${CMAKE_CURRENT_SOURCE_DIR}/web/loco-32x32.png" BINARY)
target_add_binary_data(${CMAKE_PROJECT_NAME}.elf "${CMAKE_CURRENT_SOURCE_DIR}/web/cdi.js.gz" BINARY)

###############################################################################
# Configuration validations
###############################################################################

if (NOT CONFIG_FREERTOS_HZ EQUAL 1000)
    message(FATAL_ERROR "FreeRTOS tick rate (hz) is required to be 1000.")
endif()

if (NOT CONFIG_LWIP_SO_RCVBUF)
    message(FATAL_ERROR "LwIP SO_RCVBUF is a required option in menuconfig.")
endif()

if (NOT CONFIG_PARTITION_TABLE_CUSTOM)
    message(FATAL_ERROR "The custom partition table option is not enabled in menuconfig and is required for compilation.")
endif()

if (IDF_VERSION_MAJOR LESS 5 AND CONFIG_LWIP_DHCP_OPTIONS_LEN LESS 128)
    message(FATAL_ERROR "CONFIG_LWIP_DHCP_OPTIONS_LEN should be at least 128 to workaround https://github.com/espressif/esp-idf/issues/7742")
endif()

idf_build_get_property(idf_target IDF_TARGET)

if(${idf_target} STREQUAL "esp32")
    if (NOT CONFIG_PARTITION_TABLE_FILENAME STREQUAL "ESP32CS-partitions.csv")
        message(FATAL_ERROR "The custom partition table option is not enabled in menuconfig and is required for compilation.")
    endif()

    if (NOT CONFIG_PARTITION_TABLE_CUSTOM_FILENAME STREQUAL "ESP32CS-partitions.csv")
        message(FATAL_ERROR "The custom partition table option is not enabled in menuconfig and is required for compilation.")
    endif()
    if (NOT CONFIG_ESP32_ULP_COPROC_ENABLED)
        message(FATAL_ERROR "ULP Co-Processor must be enabled")
    endif()
elseif(${idf_target} STREQUAL "esp32s3")
    if (NOT CONFIG_PARTITION_TABLE_FILENAME STREQUAL "ESP32CS-S3-partitions.csv")
        message(FATAL_ERROR "The custom partition table option is not enabled in menuconfig and is required for compilation.")
    endif()

    if (NOT CONFIG_PARTITION_TABLE_CUSTOM_FILENAME STREQUAL "ESP32CS-S3-partitions.csv")
        message(FATAL_ERROR "The custom partition table option is not enabled in menuconfig and is required for compilation.")
    endif()
    if (NOT CONFIG_ESP32S3_ULP_COPROC_ENABLED)
        message(FATAL_ERROR "ULP Co-Processor must be enabled")
    endif()
endif()
