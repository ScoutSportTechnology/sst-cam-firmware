cmake_minimum_required(VERSION 3.24)

project(sst_cam_firmware
    VERSION 0.1.0
    LANGUAGES C CXX
)

# ============================================================
# Basics
# ============================================================
set(SST_REPO_ROOT_DIR "${CMAKE_SOURCE_DIR}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if (MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

option(SST_ENABLE_TESTS "Enable tests" OFF)

# ============================================================
# Portable dependencies (Conan)
# ============================================================
add_library(sst_cam_firmware_external_libs INTERFACE)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(SQLiteCpp REQUIRED)

target_link_libraries(sst_cam_firmware_external_libs INTERFACE
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    fmt::fmt
    SQLiteCpp
)

# ============================================================
# System dependencies (JetPack via apt/pkg-config)
# Always enabled for your projects.
# ============================================================
find_package(PkgConfig REQUIRED)

pkg_search_module(GSTREAMER REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
pkg_search_module(GSTREAMER_SDP REQUIRED IMPORTED_TARGET gstreamer-sdp-1.0>=1.4)
pkg_search_module(GSTREAMER_APP REQUIRED IMPORTED_TARGET gstreamer-app-1.0>=1.4)
pkg_search_module(GSTREAMER_VIDEO REQUIRED IMPORTED_TARGET gstreamer-video-1.0>=1.4)

target_link_libraries(sst_cam_firmware_external_libs INTERFACE
    PkgConfig::GSTREAMER
    PkgConfig::GSTREAMER_SDP
    PkgConfig::GSTREAMER_APP
    PkgConfig::GSTREAMER_VIDEO
)

pkg_search_module(OPENCV REQUIRED opencv4)

# OpenCV in the JetPack sysroot is mixed: the plain `libopencv_core.so` symlink
# in /usr/lib/aarch64-linux-gnu points at 4.5d, but the imgproc/etc modules
# only exist as 4.8 (SONAME-versioned, no plain `.so`). Linking the plain
# symlinks would pull in two ABI-incompatible cores at runtime. Pin both core
# and imgproc to the matching 4.8 SONAME files explicitly.
find_library(SST_OPENCV_CORE
    NAMES libopencv_core.so.408
    REQUIRED
)
find_library(SST_OPENCV_IMGPROC
    NAMES libopencv_imgproc.so.408
    REQUIRED
)

target_include_directories(sst_cam_firmware_external_libs INTERFACE
    ${OPENCV_INCLUDE_DIRS}
)

target_link_libraries(sst_cam_firmware_external_libs INTERFACE
    ${SST_OPENCV_CORE}
    ${SST_OPENCV_IMGPROC}
)

# Control plane + live preview: BlueZ over D-Bus (sdbus-c++) and the GStreamer
# RTSP server library. Both .debs are extracted into the sysroot by
# .devcontainer/sysroot/003_install_extra_pkgs.sh.
pkg_search_module(SDBUSCPP REQUIRED IMPORTED_TARGET sdbus-c++)
pkg_search_module(GST_RTSP_SERVER REQUIRED IMPORTED_TARGET
                  gstreamer-rtsp-server-1.0>=1.4)

target_link_libraries(sst_cam_firmware_external_libs INTERFACE
    PkgConfig::SDBUSCPP
    PkgConfig::GST_RTSP_SERVER
)

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
add_executable(sst_cam_firmware src/main.cpp)

target_link_libraries(sst_cam_firmware PRIVATE
    sst_cam_firmware_internal_libs
)

# ============================================================
# Tests (Conan-provided GTest if enabled)
# ============================================================
if (SST_ENABLE_TESTS)
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
    gtest_discover_tests(sst_cam_firmware_tests DISCOVERY_MODE PRE_TEST)
endif()