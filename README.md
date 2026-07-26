# Linuxbkr UI Phototype

UI-only companion repository for **Linuxbkr**. This repository contains the
SurfaceComposer/OpenGL ES/ImGui overlay implementation, touch observer,
rendering experiments, ImGui reference template, embedded safe-build audio, and
safe-obfuscated test artifacts.

## Scope

This repository intentionally contains no partition-wiping, KPM/KPatch,
reboot, block-device write, network authorization, camera upload, or destructive
backend. The included `safe-obfuscated` build uses simulated read-only
`/dev/block/by-name/*` log entries and inert power-state controls except for the
explicit UI close button.

## Contents

- `src/`, `include/`: current Linuxbkr UI implementation;
- `references/imgui_template/`: older standalone SurfaceComposer + ImGui +
  touch reference template;
- `dist/safe-obfuscated/`: AArch64 Android safe-obfuscated ELF and shell build;
- `assets/audio/`: compressed embedded UI test audio;
- `docs/`: touch and safe-obfuscation notes;
- `tools/`: Android build and packaging helpers.

## Build

```sh
export ANDROID_NDK_HOME=/path/to/android-ndk-r28c
./tools/build_android_aarch64_host.sh safe-obfuscated
```

Artifacts are written to `dist/safe-obfuscated/`. The build targets Android
API 28, AArch64, PIE, Full RELRO, and BIND_NOW.

## Safety

Use only on devices and software for which you have authorization. The safe
profile is intended for UI testing and does not perform storage writes or
system power operations.
