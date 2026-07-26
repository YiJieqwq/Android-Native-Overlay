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

## Full-display host and window resize

The minimal example follows the original `imgui_template` model: one transparent
full-display ANativeWindow/EGL framebuffer remains stable while the visible
ImGui glass panel moves and resizes inside it. This avoids clipping caused by
shrinking SurfaceControl crop without resizing the EGL framebuffer. Both lower
handles update the ImGui window geometry immediately and preserve a 900:700
aspect ratio.
