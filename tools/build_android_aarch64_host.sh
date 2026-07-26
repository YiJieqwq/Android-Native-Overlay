#!/usr/bin/env bash
# Build an Android/AArch64 target from an AArch64 Linux host using the system
# Clang driver plus the official NDK sysroot/runtime archives. The official NDK
# host executables are x86_64-only, but their target sysroot is architecture-neutral.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
NDK=${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}
VERSION=${1:-v1.1}
API=${ANDROID_API:-28}
BUILD="$ROOT/build-aarch64-host-$VERSION"
OUT="$ROOT/dist/$VERSION"

if [[ -z "$NDK" ]]; then
  echo "error: set ANDROID_NDK_HOME to Android NDK r28c+" >&2
  exit 2
fi
PRE="$NDK/toolchains/llvm/prebuilt/linux-x86_64"
SYS="$PRE/sysroot"
RESOURCE=$(find "$PRE/lib/clang" -mindepth 1 -maxdepth 1 -type d 2>/dev/null | sort -V | tail -n 1)
for path in "$SYS/usr/include" "$SYS/usr/include/c++/v1" "$RESOURCE/lib/linux/libclang_rt.builtins-aarch64-android.a"; do
  [[ -e "$path" ]] || { echo "error: incomplete NDK asset: $path" >&2; exit 2; }
done
command -v clang >/dev/null
command -v clang++ >/dev/null
command -v ld.lld >/dev/null
command -v cmake >/dev/null
command -v ninja >/dev/null

mkdir -p "$BUILD/toolchain" "$OUT"
cat >"$BUILD/toolchain/android-clang" <<EOF
#!/bin/sh
exec $(command -v clang) --target=aarch64-linux-android$API --sysroot="$SYS" \
  -resource-dir="$RESOURCE" --rtlib=compiler-rt --unwindlib=libunwind \
  -fuse-ld=lld "\$@"
EOF
cat >"$BUILD/toolchain/android-clang++" <<EOF
#!/bin/sh
exec $(command -v clang++) --target=aarch64-linux-android$API --sysroot="$SYS" \
  -resource-dir="$RESOURCE" --rtlib=compiler-rt --unwindlib=libunwind \
  -fuse-ld=lld -nostdinc++ -isystem "$SYS/usr/include/c++/v1" \
  -static-libstdc++ "\$@"
EOF
chmod 700 "$BUILD/toolchain/android-clang" "$BUILD/toolchain/android-clang++"

cmake -S "$ROOT" -B "$BUILD" -G Ninja \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
  -DCMAKE_C_COMPILER="$BUILD/toolchain/android-clang" \
  -DCMAKE_CXX_COMPILER="$BUILD/toolchain/android-clang++" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" --target android_native_overlay -j"$(nproc)"

ELF="$OUT/android-native-overlay-$VERSION"
SCRIPT="$OUT/android-native-overlay-$VERSION.sh"
cp "$BUILD/android_native_overlay" "$ELF"
chmod 755 "$ELF"
file "$ELF"
readelf -l "$ELF" | grep -F '/system/bin/linker64'
"$ROOT/tools/package.sh" "$ELF" "$SCRIPT"

PAYLOAD_LINE=$(awk '/^__PAYLOAD_BELOW__$/ {print NR+1; exit}' "$SCRIPT")
tail -n +"$PAYLOAD_LINE" "$SCRIPT" | gzip -dc > "$BUILD/verify-payload"
cmp "$ELF" "$BUILD/verify-payload"

{
  echo "version=$VERSION"
  echo "source_commit=$(git -C "$ROOT" rev-parse HEAD)"
  echo "abi=arm64-v8a"
  echo "api=android-$API"
  echo "interpreter=/system/bin/linker64"
  (cd "$OUT" && sha256sum "$(basename "$ELF")" "$(basename "$SCRIPT")")
} > "$OUT/BUILD_INFO.txt"
(cd "$OUT" && sha256sum "$(basename "$ELF")" "$(basename "$SCRIPT")") > "$SCRIPT.sha256"
chmod 644 "$OUT/BUILD_INFO.txt" "$SCRIPT.sha256"
cat "$OUT/BUILD_INFO.txt"
