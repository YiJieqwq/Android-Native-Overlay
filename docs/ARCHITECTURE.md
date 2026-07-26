# Architecture

## Layers

1. `ANativeWindowCreator` resolves OEM/private `libgui.so` symbols and owns
   SurfaceControl-backed ANativeWindow instances.
2. `OpenGLGraphics` owns EGL display/context/window surfaces and renders Dear
   ImGui draw data with OpenGL ES 3.
3. `TouchHelperA` observes `/dev/input` without `EVIOCGRAB`, publishes queued
   snapshots, and latches the pointer that begins inside the overlay region.
4. An example supplies its own `main.cpp`, draw state, layout, assets, and close
   policy.

## Repository separation

- `template/`: reusable runtime and vendored ImGui sources.
- `examples/minimal_overlay/`: product-neutral single-surface example.
- `examples/safe_demo/`: Linuxbkr-branded safe UI integration.
- `references/imgui_template/`: preserved historical reference template.
- `dist/`: reproducible test artifacts, not runtime dependencies.

## Surface resize

Some OEMs report success from `ANativeWindow_setBuffersGeometry()` without
changing the compositor surface. The examples create an exact-size replacement
SurfaceControl and rebind the existing EGL context through
`ReplaceNativeWindow()`.
