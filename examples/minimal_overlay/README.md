# Minimal overlay example

A product-neutral Android native overlay using the reusable `template/` core.
It creates one SurfaceComposer surface, initializes OpenGL ES 3 + Dear ImGui,
observes touch without grabbing it, supports drag/fold/expand/close controls,
and displays basic display/surface information.

Build and package it from the repository root:

```sh
export ANDROID_NDK_HOME=/path/to/android-ndk-r28c
./tools/build_android_aarch64_host.sh v0.1.0
```
