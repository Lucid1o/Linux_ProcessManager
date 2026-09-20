cmake_minimum_required(VERSION 3.20)

project(Pulse
    VERSION 1.0.0
    DESCRIPTION "Application-centric process and system manager for Linux"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable testing by default
include(CTest)
option(BUILD_TESTING "Build unit tests" ON)

# Subdirectories
add_subdirectory(core)
add_subdirectory(collector)
add_subdirectory(ui)
