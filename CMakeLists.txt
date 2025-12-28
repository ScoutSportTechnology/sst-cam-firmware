cmake_minimum_required(VERSION 3.20)

project(sst_cam_firmware
    VERSION 0.1.0
    LANGUAGES C CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
#include(${CMAKE_SOURCE_DIR}/cmake/SetGStreamer.cmake)

set(SST_REPO_ROOT_DIR "${CMAKE_SOURCE_DIR}")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(OpenCV REQUIRED)
find_package(GStreamer REQUIRED)

add_library(sst_cam_firmware_external_libs INTERFACE)
target_link_libraries(sst_cam_firmware_external_libs INTERFACE
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    gstreamer::gstreamer
    opencv::opencv
)

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

add_executable(sst_cam_firmware ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
target_link_libraries(sst_cam_firmware PRIVATE
    sst_cam_firmware_internal_libs
)

#sst_cam_firmware_gstreamer_runtime(sst_cam_firmware)

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

    #sst_cam_firmware_gstreamer_runtime(sst_cam_firmware_tests)

    include(GoogleTest)
    gtest_discover_tests(sst_cam_firmware_tests)
endif()