cmake_minimum_required(VERSION 3.20)
project(Area51XR VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(A51XR_OPENXR_SDK "" CACHE PATH "Path to Khronos OpenXR-SDK checkout")
set(A51XR_ONNXRUNTIME_DIR "" CACHE PATH "Path to extracted Microsoft.ML.OnnxRuntime package")

add_library(a51xr_core
    src/core/projection.cpp
    src/core/mame_bridge.cpp
    src/core/mame_ipc.cpp
    src/core/mame_adapter.cpp
    src/core/bridge_host.cpp
    src/xr/xr_runtime.cpp
    src/xr/xr_aim.cpp
    src/xr/xr_host.cpp
    src/reconstruction/depth_mesh.cpp
    src/reconstruction/reprojection.cpp
    src/reconstruction/depth_stabilizer.cpp
    src/reconstruction/depth_reconstruction.cpp
    src/reconstruction/depth_provider.cpp
    src/reconstruction/reconstruction_engine.cpp
    src/reconstruction/reconstruction_capture.cpp
    src/reconstruction/relative_depth.cpp
    src/reconstruction/depth_anything_v2.cpp
    src/reconstruction/depth_inference.cpp
    src/reconstruction/mesh_io.cpp
    src/reconstruction/reconstruction_quality.cpp
    src/reconstruction/reconstruction_presenter.cpp
    src/reconstruction/spatial_reconstruction_host.cpp
)
target_include_directories(a51xr_core PUBLIC src include)

if(WIN32 AND A51XR_OPENXR_SDK AND EXISTS "${A51XR_OPENXR_SDK}/include/openxr/openxr.h")
    target_sources(a51xr_core PRIVATE
        src/xr/openxr_runtime_win32.cpp
        src/xr/openxr_mesh_runtime_win32.cpp)
    target_include_directories(a51xr_core PRIVATE "${A51XR_OPENXR_SDK}/include")
    target_compile_definitions(a51xr_core PUBLIC A51XR_HAS_OPENXR=1)
    target_link_libraries(a51xr_core PRIVATE d3d11 dxgi)
endif()

set(A51XR_ORT_DLL "")
if(WIN32 AND A51XR_ONNXRUNTIME_DIR AND
   EXISTS "${A51XR_ONNXRUNTIME_DIR}/build/native/include/onnxruntime_cxx_api.h" AND
   EXISTS "${A51XR_ONNXRUNTIME_DIR}/runtimes/win-x64/native/onnxruntime.lib")
    target_sources(a51xr_core PRIVATE src/reconstruction/onnx_depth_backend.cpp)
    target_include_directories(a51xr_core PRIVATE "${A51XR_ONNXRUNTIME_DIR}/build/native/include")
    target_compile_definitions(a51xr_core PUBLIC A51XR_HAS_ONNXRUNTIME=1)
    target_link_libraries(a51xr_core PRIVATE "${A51XR_ONNXRUNTIME_DIR}/runtimes/win-x64/native/onnxruntime.lib")
    set(A51XR_ORT_DLL "${A51XR_ONNXRUNTIME_DIR}/runtimes/win-x64/native/onnxruntime.dll")
endif()

if(MSVC)
    target_compile_options(a51xr_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(a51xr_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(area51xr src/main.cpp)
target_link_libraries(area51xr PRIVATE a51xr_core)

add_executable(area51xr-capture src/tools/capture_bridge_frame.cpp)
target_link_libraries(area51xr-capture PRIVATE a51xr_core)

add_executable(area51xr-generate-capture src/tools/generate_capture.cpp)
target_link_libraries(area51xr-generate-capture PRIVATE a51xr_core)

add_executable(area51xr-reconstruct src/tools/reconstruct_capture.cpp)
target_link_libraries(area51xr-reconstruct PRIVATE a51xr_core)

if(A51XR_ORT_DLL AND EXISTS "${A51XR_ORT_DLL}")
    add_executable(area51xr-depth-selftest src/tools/depth_model_selftest.cpp)
    target_link_libraries(area51xr-depth-selftest PRIVATE a51xr_core)

    foreach(target_name IN ITEMS area51xr area51xr-capture area51xr-generate-capture area51xr-reconstruct area51xr-depth-selftest)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${A51XR_ORT_DLL}" "$<TARGET_FILE_DIR:${target_name}>/onnxruntime.dll")
    endforeach()
endif()

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

    add_executable(area51xr_depth_mesh_tests tests/depth_mesh_tests.cpp)
    target_link_libraries(area51xr_depth_mesh_tests PRIVATE a51xr_core)
    add_test(NAME depth_mesh COMMAND area51xr_depth_mesh_tests)

    add_executable(area51xr_reprojection_tests tests/reprojection_tests.cpp)
    target_link_libraries(area51xr_reprojection_tests PRIVATE a51xr_core)
    add_test(NAME reprojection COMMAND area51xr_reprojection_tests)

    add_executable(area51xr_depth_stabilizer_tests tests/depth_stabilizer_tests.cpp)
    target_link_libraries(area51xr_depth_stabilizer_tests PRIVATE a51xr_core)
    add_test(NAME depth_stabilizer COMMAND area51xr_depth_stabilizer_tests)

    add_executable(area51xr_depth_reconstruction_tests tests/depth_reconstruction_tests.cpp)
    target_link_libraries(area51xr_depth_reconstruction_tests PRIVATE a51xr_core)
    add_test(NAME depth_reconstruction COMMAND area51xr_depth_reconstruction_tests)

    add_executable(area51xr_reconstruction_engine_tests tests/reconstruction_engine_tests.cpp)
    target_link_libraries(area51xr_reconstruction_engine_tests PRIVATE a51xr_core)
    add_test(NAME reconstruction_engine COMMAND area51xr_reconstruction_engine_tests)

    add_executable(area51xr_reconstruction_capture_tests tests/reconstruction_capture_tests.cpp)
    target_link_libraries(area51xr_reconstruction_capture_tests PRIVATE a51xr_core)
    add_test(NAME reconstruction_capture COMMAND area51xr_reconstruction_capture_tests)

    add_executable(area51xr_relative_depth_tests tests/relative_depth_tests.cpp)
    target_link_libraries(area51xr_relative_depth_tests PRIVATE a51xr_core)
    add_test(NAME relative_depth COMMAND area51xr_relative_depth_tests)

    add_executable(area51xr_depth_anything_v2_tests tests/depth_anything_v2_tests.cpp)
    target_link_libraries(area51xr_depth_anything_v2_tests PRIVATE a51xr_core)
    add_test(NAME depth_anything_v2 COMMAND area51xr_depth_anything_v2_tests)

    add_executable(area51xr_depth_inference_tests tests/depth_inference_tests.cpp)
    target_link_libraries(area51xr_depth_inference_tests PRIVATE a51xr_core)
    add_test(NAME depth_inference COMMAND area51xr_depth_inference_tests)

    add_executable(area51xr_mesh_io_tests tests/mesh_io_tests.cpp)
    target_link_libraries(area51xr_mesh_io_tests PRIVATE a51xr_core)
    add_test(NAME mesh_io COMMAND area51xr_mesh_io_tests)

    add_executable(area51xr_reconstruction_quality_tests tests/reconstruction_quality_tests.cpp)
    target_link_libraries(area51xr_reconstruction_quality_tests PRIVATE a51xr_core)
    add_test(NAME reconstruction_quality COMMAND area51xr_reconstruction_quality_tests)

    add_executable(area51xr_reconstruction_presenter_tests tests/reconstruction_presenter_tests.cpp)
    target_link_libraries(area51xr_reconstruction_presenter_tests PRIVATE a51xr_core)
    add_test(NAME reconstruction_presenter COMMAND area51xr_reconstruction_presenter_tests)

    add_executable(area51xr_spatial_reconstruction_host_tests tests/spatial_reconstruction_host_tests.cpp)
    target_link_libraries(area51xr_spatial_reconstruction_host_tests PRIVATE a51xr_core)
    add_test(NAME spatial_reconstruction_host COMMAND area51xr_spatial_reconstruction_host_tests)
endif()
