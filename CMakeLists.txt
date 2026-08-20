cmake_minimum_required(VERSION 3.20)
project(Area51XR VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(a51xr_core
    src/core/projection.cpp
)
target_include_directories(a51xr_core PUBLIC src)

if(MSVC)
    target_compile_options(a51xr_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(a51xr_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(area51xr src/main.cpp)
target_link_libraries(area51xr PRIVATE a51xr_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(area51xr_tests tests/projection_tests.cpp)
    target_link_libraries(area51xr_tests PRIVATE a51xr_core)
    add_test(NAME projection COMMAND area51xr_tests)
endif()
