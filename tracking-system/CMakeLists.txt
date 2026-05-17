cmake_minimum_required(VERSION 3.16)
project(tracking_system VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio imgcodecs highgui)
find_package(cppzmq CONFIG REQUIRED)
find_package(GTest REQUIRED)

add_subdirectory(src/core)
add_subdirectory(tests/cpp_unit)
