#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
NDK=${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}
if [[ -z "$NDK" || ! -f "$NDK/build/cmake/android.toolchain.cmake" ]]; then
  echo "error: set ANDROID_NDK_HOME to Android NDK r28c+" >&2
  exit 2
fi
cmake -S "$ROOT" -B "$ROOT/build" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-28 \
  -DANDROID_STL=c++_static \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT/build" --target redteam_ui -j"$(nproc)"
file "$ROOT/build/redteam_ui"
readelf -l "$ROOT/build/redteam_ui" | grep -F '/system/bin/linker64'
"$ROOT/tools/package.sh" "$ROOT/build/redteam_ui" "$ROOT/dist/redteam_ui.sh"
