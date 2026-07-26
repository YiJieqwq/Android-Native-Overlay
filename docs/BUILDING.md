# Building

## Requirements

- Android NDK r28c or newer;
- CMake 3.21+ and Ninja;
- Clang/LLD;
- AArch64 Android target, API 28 by default.

## Build and package

```sh
export ANDROID_NDK_HOME=/path/to/android-ndk-r28c
./tools/build_android_aarch64_host.sh v0.1.0
```

The helper builds `android_native_overlay`, verifies the Android linker and
self-extracting payload, and writes artifacts to `dist/v0.1.0/`.
