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
    src/xr/d3d11_mesh_renderer.cpp
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
   EXISTS "${A51XR_ONNXRUNTIME_DIR}/runtimes/win-x64/native/onnxruntime.dll")
    target_sources(a51xr_core PRIVATE src/reconstruction/onnx_depth_backend.cpp)
    target_include_directories(a51xr_core PRIVATE "${A51XR_ONNXRUNTIME_DIR}/build/native/include")
    target_compile_definitions(a51xr_core PUBLIC A51XR_HAS_ONNXRUNTIME=1)
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
    # RelWithDebInfo defines NDEBUG by default. Keep assertions active so the
    # assert-based regression sources actually execute their checks.
    if(MSVC)
        add_compile_options(/UNDEBUG)
    else()
        add_compile_options(-UNDEBUG)
    endif()

    # Compile every regression source into one executable. Windows reputation
    # scanning was intermittently refusing to launch freshly linked individual
    # test EXEs even though neighboring binaries launched normally. Renaming
    # each source's main() keeps the tests separate at compile time while CTest
    # only needs to launch one stable process.
    set(A51XR_TEST_SOURCES
        tests/projection_tests.cpp
        tests/mame_bridge_tests.cpp
        tests/mame_adapter_tests.cpp
        tests/bridge_host_tests.cpp
        tests/xr_runtime_tests.cpp
        tests/xr_aim_tests.cpp
        tests/xr_host_tests.cpp
        tests/d3d11_mesh_renderer_tests.cpp
        tests/reprojection_tests.cpp
        tests/depth_stabilizer_tests.cpp
        tests/depth_reconstruction_tests.cpp
        tests/reconstruction_engine_tests.cpp
        tests/reconstruction_capture_tests.cpp
        tests/relative_depth_tests.cpp
        tests/depth_anything_v2_tests.cpp
        tests/depth_inference_tests.cpp
        tests/mesh_io_tests.cpp
        tests/reconstruction_quality_tests.cpp
        tests/reconstruction_presenter_tests.cpp
        tests/spatial_reconstruction_host_tests.cpp
    )

    set_source_files_properties(tests/projection_tests.cpp PROPERTIES COMPILE_DEFINITIONS "main=area51xr_test_projection")
    set_source_files_properties(tests/mame_bridge_tests.cpp PROPERTIES COMPILE_DEFINITIONS "main=area51xr_test_mame_bridge")
    set_source_files_properties(tests/mame_adapter_tests.cpp PROPERTIES COMPILE_DEFINITIONS "main=area51xr_test_mame_adapter")
    set_source_files_properties(tests/bridge_host_tests.cpp PROPERTIES COMPILE_DEFINITIONS "main=area51xr_test_bridge_host")
    set_source_files_properties(tests/xr_runtime_tests.cpp PROPERTIES COMPILE_DEFINITIONS "main=area51xr_test_xr_runtime")
    set_source_files_properties(tests/xr_aim_tests.cpp PROPERTIES COMPILE_DEFINITIONS "main=area51xr_test_xr_aim")
    set_source_files_properties(tests/xr_host_tests.cpp PROPERTIES COMPILE_DEFINITIONS "main=area51xr_test_xr_host")
    set_source_files_properties(tests/d3d11_mesh_renderer_tests.cpp PROPERTIES COMPILE_DEFINITIONS "main=area51xr_test_d3d11_mesh_renderer")
    set_source_files_properties(tests/reprojection_tests.cpp PROPERTIES COMPILE_DEFINITIONS "main=area51xr_test_reprojection")
    set_source_files_properties(tests/depth_stabilizer_tests.cpp PROPERTIES COMPILE_DEFINITIONS "main=area51xr_test_depth_stabilizer")
    set_source_files_properties(tests/depth_reconstruction_tests.cpp PROPERTIES COMPILE_DEFINITIONS "main=area51xr_test_depth_reconstruction")
    set_source_files_properties(tests/reconstruction_engine_tests.cpp PROPERTIES COMPILE_DEFINITIONS "main=area51xr_test_reconstruction_engine")
    set_source_files_properties(tests/reconstruction_capture_tests.cpp PROPERTIES COMPILE_DEFINITIONS "main=area51xr_test_reconstruction_capture")
    set_source_files_properties(tests/relative_depth_tests.cpp PROPERTIES COMPILE_DEFINITIONS "main=area51xr_test_relative_depth")
    set_source_files_properties(tests/depth_anything_v2_tests.cpp PROPERTIES COMPILE_DEFINITIONS "main=area51xr_test_depth_anything_v2")
    set_source_files_properties(tests/depth_inference_tests.cpp PROPERTIES COMPILE_DEFINITIONS "main=area51xr_test_depth_inference")
    set_source_files_properties(tests/mesh_io_tests.cpp PROPERTIES COMPILE_DEFINITIONS "main=area51xr_test_mesh_io")
    set_source_files_properties(tests/reconstruction_quality_tests.cpp PROPERTIES COMPILE_DEFINITIONS "main=area51xr_test_reconstruction_quality")
    set_source_files_properties(tests/reconstruction_presenter_tests.cpp PROPERTIES COMPILE_DEFINITIONS "main=area51xr_test_reconstruction_presenter")
    set_source_files_properties(tests/spatial_reconstruction_host_tests.cpp PROPERTIES COMPILE_DEFINITIONS "main=area51xr_test_spatial_reconstruction_host")

    add_executable(area51xr_tests tests/test_driver.cpp ${A51XR_TEST_SOURCES})
    target_link_libraries(area51xr_tests PRIVATE a51xr_core)
    add_test(NAME regressions COMMAND area51xr_tests)
endif()
