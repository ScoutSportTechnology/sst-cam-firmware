cmake_minimum_required(VERSION 3.20)

project(sst_cam_firmware
    VERSION 0.1.0
    LANGUAGES CXX
)

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

find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED IMPORTED_TARGET
    gstreamer-1.0
    gstreamer-base-1.0
    gstreamer-app-1.0
    gstreamer-video-1.0
)

# External deps bundle target (NO sources)
add_library(sst_cam_firmware_external_libs INTERFACE)
target_link_libraries(sst_cam_firmware_external_libs INTERFACE
    PkgConfig::GST
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

# --- Core library (everything except main.cpp) ---
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

# Remove app entry point from the core library
list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

add_library(sst_cam_firmware_internal_libs STATIC ${CORE_SOURCES})

# IMPORTANT: PUBLIC so anything that links this lib (app/tests) inherits include path
target_include_directories(sst_cam_firmware_internal_libs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# --- App executable ---
add_executable(sst_cam_firmware ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
target_link_libraries(sst_cam_firmware PRIVATE
    sst_cam_firmware_internal_libs
    sst_cam_firmware_external_libs
)

# --- Tests ---
option(BUILD_TESTS "Build unit tests" OFF)
if (BUILD_TESTS)
    enable_testing()

    find_package(GTest CONFIG REQUIRED)

    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
    )

    add_executable(unit_tests ${TEST_SOURCES})

    target_link_libraries(unit_tests PRIVATE
        sst_cam_firmware_internal_libs
        sst_cam_firmware_external_libs
        GTest::gtest
        GTest::gtest_main
    )

    # Pass repo root to tests so they can open test JSON fixtures reliably
    target_compile_definitions(unit_tests PRIVATE
        SST_REPO_ROOT="${CMAKE_SOURCE_DIR}"
    )

    include(GoogleTest)
    gtest_discover_tests(unit_tests)
endif()
