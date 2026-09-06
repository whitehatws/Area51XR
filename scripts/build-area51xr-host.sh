#!/usr/bin/env bash
set -euo pipefail

: "${A51XR_ROOT_MSYS:?A51XR_ROOT_MSYS is required}"
: "${A51XR_OPENXR_SDK_MSYS:?A51XR_OPENXR_SDK_MSYS is required}"
: "${A51XR_ONNXRUNTIME_DIR_MSYS:?A51XR_ONNXRUNTIME_DIR_MSYS is required}"

export PATH=/ucrt64/bin:/usr/bin:$PATH

cmake -S "$A51XR_ROOT_MSYS" \
    -B "$A51XR_ROOT_MSYS/build-mingw" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DA51XR_OPENXR_SDK="$A51XR_OPENXR_SDK_MSYS" \
    -DA51XR_ONNXRUNTIME_DIR="$A51XR_ONNXRUNTIME_DIR_MSYS"

cmake --build "$A51XR_ROOT_MSYS/build-mingw" --parallel
