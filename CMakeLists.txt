cmake_minimum_required(VERSION 3.20)

project(sst_cam_firmware
    VERSION 0.1.0
    LANGUAGES C CXX
)

set(SST_REPO_ROOT_DIR "${CMAKE_SOURCE_DIR}")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)

# ============================================================
# Platform selection (Jetson / Raspberry Pi only)
# ============================================================
set(SST_PLATFORM "jetson" CACHE STRING "Target platform: jetson|rpi")
set_property(CACHE SST_PLATFORM PROPERTY STRINGS jetson rpi)

# These two platforms both use GStreamer + OpenCV (system packages)
option(SST_WITH_GSTREAMER "Enable GStreamer integration" ON)
option(SST_WITH_OPENCV "Enable OpenCV integration" ON)

message(STATUS "SST_PLATFORM=${SST_PLATFORM}")
message(STATUS "SST_WITH_GSTREAMER=${SST_WITH_GSTREAMER}")
message(STATUS "SST_WITH_OPENCV=${SST_WITH_OPENCV}")

# ============================================================
# External libs (portable + platform)
# ============================================================
add_library(sst_cam_firmware_external_libs INTERFACE)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

target_link_libraries(sst_cam_firmware_external_libs INTERFACE
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    fmt::fmt
)

# ============================================================
# GStreamer + OpenCV (Linux only via pkg-config)
# ============================================================
find_package(PkgConfig REQUIRED)

if (SST_WITH_GSTREAMER)
    pkg_search_module(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
    pkg_search_module(gstreamer_sdp REQUIRED IMPORTED_TARGET gstreamer-sdp-1.0>=1.4)
    pkg_search_module(gstreamer_app REQUIRED IMPORTED_TARGET gstreamer-app-1.0>=1.4)
    pkg_search_module(gstreamer_video REQUIRED IMPORTED_TARGET gstreamer-video-1.0>=1.4)

    target_link_libraries(sst_cam_firmware_external_libs INTERFACE
        PkgConfig::gstreamer
        PkgConfig::gstreamer_sdp
        PkgConfig::gstreamer_app
        PkgConfig::gstreamer_video
    )

    target_compile_definitions(sst_cam_firmware_external_libs INTERFACE
        SST_HAS_GSTREAMER=1
    )
else()
    target_compile_definitions(sst_cam_firmware_external_libs INTERFACE
        SST_HAS_GSTREAMER=0
    )
endif()

if (SST_WITH_OPENCV)
    pkg_search_module(opencv REQUIRED IMPORTED_TARGET opencv4)

    target_link_libraries(sst_cam_firmware_external_libs INTERFACE
        PkgConfig::opencv
    )

    target_compile_definitions(sst_cam_firmware_external_libs INTERFACE
        SST_HAS_OPENCV=1
    )
else()
    target_compile_definitions(sst_cam_firmware_external_libs INTERFACE
        SST_HAS_OPENCV=0
    )
endif()

# Platform macros (for #ifdef in code)
target_compile_definitions(sst_cam_firmware_external_libs INTERFACE
  SST_PLATFORM_JETSON=0
  SST_PLATFORM_RPI=1
)
if (SST_PLATFORM STREQUAL "jetson")
  target_compile_definitions(sst_cam_firmware_external_libs INTERFACE SST_PLATFORM_ID=SST_PLATFORM_JETSON)
elseif (SST_PLATFORM STREQUAL "rpi")
  target_compile_definitions(sst_cam_firmware_external_libs INTERFACE SST_PLATFORM_ID=SST_PLATFORM_RPI)
else()
  message(FATAL_ERROR "SST_PLATFORM must be 'jetson' or 'rpi'")
endif()

# ============================================================
# Internal library
# ============================================================
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)
list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

add_library(sst_cam_firmware_internal_libs STATIC ${CORE_SOURCES})

target_link_libraries(sst_cam_firmware_internal_libs PUBLIC
    sst_cam_firmware_external_libs
)

target_include_directories(sst_cam_firmware_internal_libs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(sst_cam_firmware_internal_libs PUBLIC
    SST_REPO_ROOT_DIR="${SST_REPO_ROOT_DIR}"
)

# ============================================================
# Main executable
# ============================================================
add_executable(sst_cam_firmware ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

target_link_libraries(sst_cam_firmware PRIVATE
    sst_cam_firmware_internal_libs
)

# ============================================================
# Tests
# ============================================================
option(BUILD_TESTS "Build tests" OFF)
if (BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)

    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
    )

    add_executable(sst_cam_firmware_tests ${TEST_SOURCES})
    target_link_libraries(sst_cam_firmware_tests PRIVATE
        sst_cam_firmware_internal_libs
        GTest::gtest
        GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(sst_cam_firmware_tests)
endif()