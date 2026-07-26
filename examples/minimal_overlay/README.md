# Minimal overlay example

A small, product-neutral Android native overlay using the reusable `template/`
core. It creates one SurfaceComposer surface, initializes OpenGL ES 3 + Dear
ImGui, observes touch without grabbing it, displays basic display/surface
information, and exits through a normal Close button.

Build the target after configuring the project:

```sh
cmake --build build-aarch64-host-minimal --target minimal_overlay -j
```

The default packaging helper still builds the safe Linuxbkr demo target.
