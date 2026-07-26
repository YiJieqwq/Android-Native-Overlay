# Android native overlay template core

This directory contains reusable SurfaceComposer, EGL/OpenGL ES 3, Dear ImGui
backend, and non-exclusive touch observer code. Product-specific drawing,
layout, branding, and assets belong in an example under `examples/`.

`examples/safe_demo/` is the reference integration and supplies the current
`draw.h`/`draw_Gui.cpp` implementation.
