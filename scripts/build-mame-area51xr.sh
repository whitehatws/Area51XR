#!/usr/bin/env bash
set -euo pipefail

: "${A51XR_MAME_ROOT_MSYS:?A51XR_MAME_ROOT_MSYS is required}"
: "${A51XR_MAME_JOBS:?A51XR_MAME_JOBS is required}"

export OS=Windows_NT
export MSYSTEM=UCRT64
export MINGW_PREFIX=/ucrt64
export MINGW_CHOST=x86_64-w64-mingw32
export MINGW_PACKAGE_PREFIX=mingw-w64-ucrt-x86_64
export PATH=/ucrt64/bin:/usr/bin:$PATH

printf 'MAME toolchain: MSYSTEM=%s MINGW_PREFIX=%s\n' "$MSYSTEM" "$MINGW_PREFIX"
cd "$A51XR_MAME_ROOT_MSYS"

PROJECT_MAKEFILE="build/projects/windows/mamearea51xr/gmake-mingw64-gcc/Makefile"
if [[ -f "$PROJECT_MAKEFILE" ]]; then
    echo "Using existing generated MAME project files for incremental build."
    exec make SUBTARGET=area51xr SOURCES=src/mame/atari/jaguar.cpp IGNORE_GIT=1 -j"$A51XR_MAME_JOBS"
fi

echo "Generated MAME project files are missing; repairing Genie before regeneration."
GENIE="3rdparty/genie/bin/windows/genie.exe"
if [[ -f "$GENIE" ]]; then
    chmod +x "$GENIE" 2>/dev/null || true
fi

if ! "$GENIE" --help >/dev/null 2>&1; then
    rm -f "$GENIE"
    make -C 3rdparty/genie PROJECT_TYPE=gmake -j"$A51XR_MAME_JOBS"
    chmod +x "$GENIE" 2>/dev/null || true
fi

if ! "$GENIE" --help >/dev/null 2>&1; then
    echo "ERROR: MAME Genie generator is still not runnable after local rebuild." >&2
    exit 126
fi

exec make SUBTARGET=area51xr SOURCES=src/mame/atari/jaguar.cpp IGNORE_GIT=1 REGENIE=1 -j"$A51XR_MAME_JOBS"
