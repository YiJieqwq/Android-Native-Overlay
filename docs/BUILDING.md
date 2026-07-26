# Building

## Requirements

- Android NDK r28c or newer;
- CMake 3.21+ and Ninja;
- Clang/LLD;
- AArch64 Android target, API 28 by default.

## Safe demo

```sh
export ANDROID_NDK_HOME=/path/to/android-ndk-r28c
./tools/build_android_aarch64_host.sh safe-obfuscated
```

## Minimal example

Configure through the helper, then build the explicit target:

```sh
export ANDROID_NDK_HOME=/path/to/android-ndk-r28c
./tools/build_android_aarch64_host.sh minimal-config
cmake --build build-aarch64-host-minimal-config --target minimal_overlay -j2
```

The helper currently packages the safe-demo target; the prebuilt minimal
launcher is under `dist/minimal-overlay/`.
