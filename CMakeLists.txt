cmake_minimum_required(VERSION 3.10.0)
project(practicalproject VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

# Find SFML 3
find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System)

add_executable(practicalproject main.cpp)

target_link_libraries(practicalproject PRIVATE
    SFML::Graphics
    SFML::Window
    SFML::System
)