## Release v0.1.0 — First stable prototype

v0.1.0 is created and tagged. The API (api.github.com) is unreachable from this
sandbox, so the release body and asset upload must be done manually.

### Option A — Use the GitHub web UI

1. Open https://github.com/YiJieqwq/Linuxbkr/releases
2. "Draft a new release" → choose tag **v0.1.0**
3. Title: `v0.1.0 — First stable prototype`
4. Attach `dist/redteam_ui_blur_switch_150.sh` (1.7 MiB)

### Option B — gh CLI

```bash
gh release create v0.1.0 \
  --repo YiJieqwq/Linuxbkr \
  --title "v0.1.0 — First stable prototype" \
  --notes-file - <<'NOTES'

## RedTeam ImGui SurfaceComposer UI

Safe, production-quality native Android overlay prototype.

### Features
- **1000×1200** draggable floating window, collapses to **1000×150**
- Xiaomi SurfaceFlinger **background blur** with switchable fixed-size layers
- **Absolute touch-drag** (no MouseDelta); collapse arrow drawn with AddLine()
- **Finger-scroll** log with auto-follow that pauses on manual scroll
- Dark graphite dynamic flow gradient (3 moving translucent ribbons)
- Read-only touch observer — no EVIOCGRAB, no uinput, no input monopoly
- Embedded OPPO Sans font for Chinese labels

### Safety
No /dev/block access, KSU module deployment, KPM control, destructive IOCTL,
reboot/sysrq, panic, input grabbing, or executable command backend.

### Build
```sh
export ANDROID_NDK_HOME=/path/to/android-ndk-r28c
./tools/build_android.sh
```

### Deploy
```sh
adb push dist/redteam_ui_blur_switch_150.sh /sdcard/
adb shell su -c 'sh /sdcard/redteam_ui_blur_switch_150.sh'
```
NOTES \
  dist/redteam_ui_blur_switch_150.sh
```

Push status: ✅ tag v0.1.0 is on GitHub; repo is in sync.
