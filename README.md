# Android Native Overlay Template

**Linuxbkr UI Phototype** is an Android native overlay template based on
SurfaceComposer, OpenGL ES 3, Dear ImGui, EGL, and a non-exclusive touch
observer.

## What it provides

- Native SurfaceComposer/ANativeWindow overlay creation;
- OpenGL ES 3 + Dear ImGui rendering;
- non-exclusive touch observation with pointer latching and event queuing;
- runtime surface resize/rebinding for foldable panels;
- optional compositor blur and rounded UI drawing;
- AArch64 Android PIE build and self-extracting shell packaging;
- a safe demo with simulated, read-only `/dev/block/by-name/*` log entries.

## Repository layout

```text
template/                       reusable overlay runtime
examples/safe_demo/             Linuxbkr safe UI example and audio
examples/minimal_overlay/       minimal-example starting point
references/imgui_template/      older standalone ImGui reference
dist/safe-obfuscated/            built safe demo artifacts
third_party/miniaudio/           embedded audio dependency
tools/                           Android build/package helpers
docs/                            architecture and safety notes
```

## Build

The default CMake target builds the safe demo while using the reusable runtime
from `template/`:

```sh
export ANDROID_NDK_HOME=/path/to/android-ndk-r28c
./tools/build_android_aarch64_host.sh safe-obfuscated
```

The output is written to:

```text
dist/safe-obfuscated/
```

The target is Android API 28, AArch64, PIE, Full RELRO, and BIND_NOW.

## Safety scope

The safe demo contains no partition-writing, KPM/KPatch, reboot, destructive
command, or network authorization backend. Its power-state buttons are inert;
the explicit UI close button only exits the demo.

Use this template only on devices and software for which you have authorization.
See `docs/SAFE_OBFUSCATION.md`, `docs/TOUCH_INPUT.md`, and
`template/README.md` for details.

## Documentation

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
- [`docs/BUILDING.md`](docs/BUILDING.md)
- [`docs/PLATFORM_NOTES.md`](docs/PLATFORM_NOTES.md)
- [`docs/TOUCH_INPUT.md`](docs/TOUCH_INPUT.md)
- [`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md)

The project uses private SurfaceComposer ABI and must be validated per OEM.
See the tagged-pointer cleanup note in `docs/PLATFORM_NOTES.md`.
