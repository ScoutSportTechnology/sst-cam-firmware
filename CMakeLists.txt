cmake_minimum_required(VERSION 3.20)

project(sst_cam_firmware
    VERSION 0.1.0
    LANGUAGES CXX
)

# Absolute repo root (compile-time) for resolving config + test fixture paths
set(SST_REPO_ROOT_DIR "${CMAKE_SOURCE_DIR}")

# Optional: enforce Clang only
if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR
        "This project must be built with Clang. "
        "Current compiler is: ${CMAKE_CXX_COMPILER_ID}"
    )
endif()

# C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Warnings
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
)

# --- Dependencies (Conan/CMakeDeps) ---
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# --- via pkg-config ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED IMPORTED_TARGET
    gstreamer-1.0
    gstreamer-base-1.0
    gstreamer-app-1.0
    gstreamer-video-1.0
    gstreamer-webrtc-1.0
    gstreamer-sdp-1.0
    gstreamer-pbutils-1.0
    gstreamer-rtsp-server-1.0
)

pkg_check_modules(OPENCV REQUIRED IMPORTED_TARGET opencv4)

# External deps bundle target (NO sources)
add_library(sst_cam_firmware_external_libs INTERFACE)
target_link_libraries(sst_cam_firmware_external_libs INTERFACE
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    PkgConfig::GST
    PkgConfig::OPENCV
)


# --- Core library (everything except main.cpp) ---
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

# Remove app entry point from the core library
list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

add_library(sst_cam_firmware_internal_libs STATIC ${CORE_SOURCES})
target_link_libraries(sst_cam_firmware_internal_libs PUBLIC
    sst_cam_firmware_external_libs
)

# IMPORTANT: PUBLIC so anything that links this lib (app/tests) inherits include path
target_include_directories(sst_cam_firmware_internal_libs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Provide repo root to anything linking the core library (app + tests)
target_compile_definitions(sst_cam_firmware_internal_libs PUBLIC
    SST_REPO_ROOT_DIR="${SST_REPO_ROOT_DIR}"
)

# --- App executable ---
add_executable(sst_cam_firmware ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
target_link_libraries(sst_cam_firmware PRIVATE
    sst_cam_firmware_internal_libs
)

# --- Tests ---
option(BUILD_TESTS "Build unit tests" OFF)
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

# --- compile_commands.json convenience copy ---
if (CMAKE_EXPORT_COMPILE_COMMANDS)
    add_custom_command(TARGET sst_cam_firmware POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/compile_commands.json
            ${CMAKE_SOURCE_DIR}/compile_commands.json
    )

    if (BUILD_TESTS)
        add_custom_command(TARGET sst_cam_firmware_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_BINARY_DIR}/compile_commands.json
                ${CMAKE_SOURCE_DIR}/compile_commands.json
        )
    endif()
endif()
