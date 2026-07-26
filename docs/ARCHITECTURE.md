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

## Fixed logical producer and live layer transform

The example keeps a fixed 900x828 EGL producer: 900x700 for the glass panel,
64 logical pixels above for the HyperOS-style size hint, and 64 below for the
external resize handles. ImGui always renders the complete logical scene, so
no widget or text is clipped during resize.

Live proportional resize applies a SurfaceControl matrix and crop to the whole
layer. SurfaceFlinger therefore scales framebuffer content and input bounds
together while the EGL buffer remains stable. This avoids both the clipped
crop behavior and the touch interception of a full-display producer.
