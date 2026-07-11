cmake_minimum_required(VERSION 3.16)
project(tracking_core
    VERSION 0.3.0
    DESCRIPTION "Drone Ball Retrieval — Tracking Core"
    LANGUAGES CXX
)

# ─── Language standard (ADR-001: C++17 minimum) ───────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─── Compiler warnings (zero-warning policy) ─────────────────────────────────
add_compile_options(-Wall -Wextra -Wpedantic)

# ─── System dependencies ─────────────────────────────────────────────────────
# OpenCV: too large for FetchContent — must be system-installed.
# Pi 5: sudo apt install libopencv-dev
# Dev:   sudo apt install libopencv-dev (or brew install opencv)
find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio calib3d)

# ─── FetchContent dependencies ───────────────────────────────────────────────
include(FetchContent)

# cppzmq — header-only C++ ZMQ bindings (ADR-002)
FetchContent_Declare(cppzmq
    GIT_REPOSITORY https://github.com/zeromq/cppzmq.git
    GIT_TAG        v4.10.0
)
set(CPPZMQ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cppzmq)

# nlohmann/json — single-header JSON serialisation
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(json)

# yaml-cpp — YAML configuration parsing (startup only, not hot-path)
FetchContent_Declare(yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG        0.8.0
)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml-cpp)

# spdlog — async ring-buffered logger (ADR-001: no sync I/O on hot-path)
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
)
FetchContent_MakeAvailable(spdlog)

# GoogleTest — unit testing framework
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ─── Testing ─────────────────────────────────────────────────────────────────
# Required at the top level so gtest_discover_tests registrations in
# tests/cpp_unit are visible to `ctest` run from the build root.
enable_testing()

# ─── Subdirectories ──────────────────────────────────────────────────────────
add_subdirectory(src/core)
add_subdirectory(tests/cpp_unit)
