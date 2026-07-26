# Android Native Overlay

A minimal Android native overlay template built with SurfaceComposer,
ANativeWindow/EGL, OpenGL ES 3, Dear ImGui, and a non-exclusive `/dev/input`
touch observer.

## Features

- AArch64 Android native executable; no Activity or Java UI required;
- SurfaceComposer-backed transparent overlay;
- OpenGL ES 3 + Dear ImGui;
- non-exclusive touch observation;
- draggable title region, fold/expand control, and close control;
- runtime Surface replacement with EGL context reuse;
- self-extracting shell packaging.

## Layout

```text
template/                   reusable runtime
examples/minimal_overlay/   minimal UI integration
tools/                      build and package helpers
docs/                       architecture and platform notes
```

## Build

```sh
export ANDROID_NDK_HOME=/path/to/android-ndk-r28c
./tools/build_android_aarch64_host.sh v0.1.0
```

## Run

Download the shell asset from the latest GitHub Release, push it to an
authorized rooted test device, then run it with a shell that can access the
required SurfaceComposer and input interfaces.

This project relies on private Android/OEM `libgui.so` ABI. Read
[`docs/PLATFORM_NOTES.md`](docs/PLATFORM_NOTES.md) before use.
