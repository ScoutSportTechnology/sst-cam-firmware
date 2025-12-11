cmake_minimum_required(VERSION 3.20)

project(sst_cam_firmware
    VERSION 0.1.0
    LANGUAGES CXX
)

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

# --- Dependencies (resolved by vcpkg) ---
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(GStreamer REQUIRED)

# --- Sources ---
file(GLOB_RECURSE SRC_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_executable(sst_cam ${SRC_FILES})

target_include_directories(sst_cam
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(sst_cam
    PRIVATE
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        GStreamer::gstreamer-1.0
)
