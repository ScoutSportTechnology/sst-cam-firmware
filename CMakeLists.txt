cmake_minimum_required(VERSION 3.20)

project(sst_cam_firmware
    VERSION 0.1.0
    LANGUAGES CXX
)

# --- Global C++ settings ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# All executables will go to build/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- External dependency via FetchContent ---
include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(nlohmann_json)

# --- Source files (all .cpp under src/) ---
file(GLOB_RECURSE SRC_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

# --- Executable target ---
add_executable(sst_cam
    ${SRC_FILES}
)

# Where to find headers for these cpp files
target_include_directories(sst_cam
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link with external libraries
target_link_libraries(sst_cam
    PRIVATE
        nlohmann_json::nlohmann_json
)
