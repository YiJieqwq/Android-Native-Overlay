# Architecture

## Layers

1. `ANativeWindowCreator` resolves private/OEM `libgui.so` symbols and owns
   SurfaceControl-backed ANativeWindow instances.
2. `OpenGLGraphics` owns EGL display/context/window surfaces and renders Dear
   ImGui draw data with OpenGL ES 3.
3. `TouchHelperA` observes `/dev/input` without `EVIOCGRAB`, publishes queued
   snapshots, and latches a pointer that begins inside the overlay region.
4. `examples/minimal_overlay/` supplies the entry point, draw state, controls,
   and close policy.

## Repository layout

- `template/`: reusable runtime and vendored Dear ImGui sources.
- `examples/minimal_overlay/`: product-neutral single-surface example.
- `tools/`: NDK build and self-extracting shell packaging.
- `docs/`: architecture, build, touch, and platform notes.

## Surface resize

Some OEMs report success from `ANativeWindow_setBuffersGeometry()` without
changing the compositor surface. The example creates an exact-size replacement
SurfaceControl and rebinds the existing EGL context through
`ReplaceNativeWindow()`.
