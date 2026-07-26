# Security and platform notes

## Overlay behavior

The supplied template creates a privileged native SurfaceComposer surface and, on Android 12+, attempts `setTrustedOverlay(true)`. It resolves private `libgui.so` C++ symbols at runtime. This is OEM/version-sensitive and may fail when symbol signatures change.

This safe prototype deliberately does **not** implement anti-preemption, input monopoly, navigation suppression, repeated layer reassertion, or a watchdog that fights other overlays. It remains dismissible with an explicit Exit control. `Touch::Init(..., true)` selects observer mode, so the EVIOCGRAB/uinput branch is not executed.

`trusted overlay` is not synonymous with an absolute, irrevocable top-most guarantee. SurfaceFlinger policy and other privileged surfaces may still determine final ordering.

## Build environment

Ubuntu Base provides a glibc userspace while the output must be an Android/Bionic ELF with `/system/bin/linker64`. A normal Ubuntu `clang++` build is therefore not a valid substitute for the Android NDK toolchain.

Use Android NDK r28c or newer. Official Google Linux NDK host executables are
x86_64-hosted and cannot run directly in this AArch64 PRoot. For this workspace,
`tools/build_android_aarch64_host.sh` uses the native system Clang driver with
the NDK's target sysroot, libc++, compiler-rt and libunwind assets. The produced
ELF is still an Android/Bionic AArch64 PIE with `/system/bin/linker64`; it is not
a glibc host binary. Final behavior must still be validated on an Android device
because private SurfaceComposer symbols are OEM/version-sensitive.
