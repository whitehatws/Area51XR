cmake_minimum_required(VERSION 3.20)
project(Area51XR VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(A51XR_OPENXR_SDK "" CACHE PATH "Path to Khronos OpenXR-SDK checkout")

add_library(a51xr_core
    src/core/projection.cpp
    src/core/mame_bridge.cpp
    src/core/mame_ipc.cpp
    src/core/mame_adapter.cpp
    src/core/bridge_host.cpp
    src/xr/xr_runtime.cpp
    src/xr/xr_aim.cpp
    src/xr/xr_host.cpp
)
target_include_directories(a51xr_core PUBLIC src include)

if(WIN32 AND A51XR_OPENXR_SDK AND EXISTS "${A51XR_OPENXR_SDK}/include/openxr/openxr.h")
    target_sources(a51xr_core PRIVATE src/xr/openxr_runtime_win32.cpp)
    target_include_directories(a51xr_core PRIVATE "${A51XR_OPENXR_SDK}/include")
    target_compile_definitions(a51xr_core PUBLIC A51XR_HAS_OPENXR=1)
    target_link_libraries(a51xr_core PRIVATE d3d11 dxgi)
endif()

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

    add_executable(area51xr_mame_bridge_tests tests/mame_bridge_tests.cpp)
    target_link_libraries(area51xr_mame_bridge_tests PRIVATE a51xr_core)
    add_test(NAME mame_bridge COMMAND area51xr_mame_bridge_tests)

    add_executable(area51xr_mame_ipc_tests tests/mame_ipc_tests.cpp)
    target_link_libraries(area51xr_mame_ipc_tests PRIVATE a51xr_core)
    add_test(NAME mame_ipc COMMAND area51xr_mame_ipc_tests)

    add_executable(area51xr_mame_adapter_tests tests/mame_adapter_tests.cpp)
    target_link_libraries(area51xr_mame_adapter_tests PRIVATE a51xr_core)
    add_test(NAME mame_adapter COMMAND area51xr_mame_adapter_tests)

    add_executable(area51xr_bridge_host_tests tests/bridge_host_tests.cpp)
    target_link_libraries(area51xr_bridge_host_tests PRIVATE a51xr_core)
    add_test(NAME bridge_host COMMAND area51xr_bridge_host_tests)

    add_executable(area51xr_xr_runtime_tests tests/xr_runtime_tests.cpp)
    target_link_libraries(area51xr_xr_runtime_tests PRIVATE a51xr_core)
    add_test(NAME xr_runtime COMMAND area51xr_xr_runtime_tests)

    add_executable(area51xr_xr_aim_tests tests/xr_aim_tests.cpp)
    target_link_libraries(area51xr_xr_aim_tests PRIVATE a51xr_core)
    add_test(NAME xr_aim COMMAND area51xr_xr_aim_tests)

    add_executable(area51xr_xr_host_tests tests/xr_host_tests.cpp)
    target_link_libraries(area51xr_xr_host_tests PRIVATE a51xr_core)
    add_test(NAME xr_host COMMAND area51xr_xr_host_tests)
endif()
