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

The example keeps a fixed 964x796 EGL producer: a 900x700 glass panel, a
64-pixel transparent strip above for the size hint, and compact 32-pixel side
and lower margins for resize handles. The glass content is rendered inside a
clipped child region, so transparent control margins never affect content
alignment. ImGui always renders the complete logical scene, so no widget or
text is clipped during resize.

Touch latching uses one rectangular glass region plus two 30-pixel-radius
circles centered on the lower corner affordances. Transparent producer margins
outside those regions are not treated as overlay controls.

Live proportional resize applies a SurfaceControl matrix and crop to the whole
layer. SurfaceFlinger therefore scales framebuffer content and input bounds
together while the EGL buffer remains stable. This avoids both the clipped
crop behavior and the touch interception of a full-display producer.
