# Contributing

Keep reusable runtime changes under `template/` and product-specific UI under
`examples/`. New examples must remain non-destructive and should build as a
separate CMake target. Before submitting a change:

```sh
git diff --check
cmake --build <build-dir> --target minimal_overlay
```

Document OEM/private-API assumptions and do not commit credentials, device
identifiers, APK samples, or build directories.
