#!/system/bin/sh
set -eu
printf '\033[38;5;99m\n[+] Android Native Overlay template\n[+] Starting minimal overlay...\n\033[0m'
DIR=${TMPDIR:-/data/local/tmp}
BIN="$DIR/.android_native_overlay_$$"
cleanup() { rm -f "$BIN"; }
trap cleanup EXIT INT TERM
PAYLOAD_LINE=$(awk '/^__PAYLOAD_BELOW__$/ {print NR+1; exit}' "$0")
[ -n "$PAYLOAD_LINE" ] || { echo '[-] embedded payload missing' >&2; exit 2; }
tail -n +"$PAYLOAD_LINE" "$0" | gzip -dc > "$BIN"
chmod 700 "$BIN"
"$BIN" "$@"
exit $?
__PAYLOAD_BELOW__
