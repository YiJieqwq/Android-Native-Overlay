# Safe obfuscated build

The `dev/safe-obfuscation` path is UI-only. It does not contain a block-device,
reboot, KPM, KSU, command-execution, input-grab, or destructive backend.

## v1: compile-time UI string encoding

`include/My_Utils/obfuscated_strings.h` stores selected labels and synthetic log
messages as compile-time XOR-encoded byte arrays. Text is decoded only when the
UI requests it. `/dev/block/by-name/*` entries are enumerated read-only for the
simulation log; no node is opened for writing and no IOCTL is issued.

This is intentionally a modest first layer. It removes direct plaintext UI
labels and fixed partition-log lines from ordinary `strings` output, but it is
not cryptographic secret storage. Any text rendered by the process can still be
observed dynamically.

## Protected surface

- Linuxbkr branding and UI labels;
- synthetic wipe-log prefixes and suffixes;
- read-only `/dev/block/by-name` simulation path generation;
- inert `关机`, `重启`, and `无法关闭` controls;
- decorative top-left close control remains inert.

## Not obfuscated

The high-frequency ImGui/OpenGL/touch paths remain ordinary code to preserve
latency and OEM compatibility. A future O-MVLL/VMProtect profile should protect
only small, low-frequency policy functions after the plain safe build has been
validated on devices.

## Build

```sh
export ANDROID_NDK_HOME=/path/to/android-ndk-r28c
./tools/build_android_aarch64_host.sh safe-obfuscated
```

Outputs are written to `dist/safe-obfuscated/`. Verify the raw ELF and shell
payload using the generated `.sha256` file before device testing.

## Embedded audio

The safe profile embeds the reduced `RAPTURE_96k.mp3` asset and starts it through
miniaudio/OpenSLES after EGL, ImGui, and fonts are initialized. Playback loops at
45% volume and is stopped before renderer shutdown. The original 320-kbps source
is kept outside the repository; the checked-in asset is 96 kbps and has no ID3
metadata.

The bundled audio is trimmed to remove the first 17 seconds and the final 5
seconds of the source track. Playback therefore starts at the requested content
section and ends without the trailing five seconds.
