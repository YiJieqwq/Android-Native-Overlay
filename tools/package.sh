#!/usr/bin/env bash
set -euo pipefail
BIN=${1:?usage: package.sh <android-arm64-elf> [output.sh]}
OUT=${2:-dist/android-native-overlay.sh}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$(dirname "$OUT")"
awk 'BEGIN{p=1} /^__PAYLOAD_BELOW__$/{print; exit} {print}' "$ROOT/run_ui.sh" > "$OUT"
gzip -n -9 -c "$BIN" >> "$OUT"
chmod 755 "$OUT"
sha256sum "$BIN" "$OUT" > "$OUT.sha256"
