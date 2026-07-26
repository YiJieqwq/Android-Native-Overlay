#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
NDK=${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}
[[ -n "$NDK" ]] || { echo 'error: set ANDROID_NDK_HOME' >&2; exit 2; }
cmake -S "$ROOT" -B "$ROOT/build" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT/build" --target android_native_overlay -j"$(nproc)"
"$ROOT/tools/package.sh" "$ROOT/build/android_native_overlay" \
  "$ROOT/dist/android-native-overlay.sh"
