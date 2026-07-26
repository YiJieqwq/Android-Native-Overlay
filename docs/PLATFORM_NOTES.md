# Platform notes

## Private Android APIs

The runtime resolves private `libgui.so` C++ symbols. Symbol availability and
ABI layout vary by Android release and OEM, so successful compilation does not
guarantee device compatibility.

## Input

The examples use `Touch::Init(..., true)`, the non-exclusive observer path. They
do not call `EVIOCGRAB`, create uinput devices, suppress navigation, or fight
other overlays.

## Tagged pointers

Some Android/OEM combinations report a tagged-pointer warning while releasing a
Surface obtained through private SurfaceComposer wrappers. The visible overlay
has already exited, but bionic may abort during final private-object teardown.
This is a known limitation of relying on private ABI pointer layouts. Prefer
public `ANativeWindow_release()` paths, preserve top-byte tags, and test cleanup
on each target OEM.

## Build environment

The output must be an Android/Bionic ELF using `/system/bin/linker64`. A normal
Linux/glibc build is not a valid substitute. `tools/build_android_aarch64_host.sh`
uses the NDK target sysroot, libc++, compiler-rt, and libunwind assets with a
native AArch64 Clang driver.
