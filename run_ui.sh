#!/system/bin/sh
set -eu
printf '\033[38;5;99m'
cat <<'BANNER'
╔════════════════════════════════════════════════════╗
║                                                    ║
║                  L I N U X B K R                   ║
║                                                    ║
╚════════════════════════════════════════════════════╝
BANNER
printf '\033[0m\n[+] Linuxbkr UI prototype\n[+] Safe build: storage backend disconnected\n[+] Starting overlay...\n'
DIR=${TMPDIR:-/data/local/tmp}
BIN="$DIR/.linuxbkr_ui_$$"
cleanup() { rm -f "$BIN"; }
trap cleanup EXIT INT TERM
PAYLOAD_LINE=$(awk '/^__PAYLOAD_BELOW__$/ {print NR+1; exit}' "$0")
[ -n "$PAYLOAD_LINE" ] || { echo '[-] embedded payload missing' >&2; exit 2; }
tail -n +"$PAYLOAD_LINE" "$0" | gzip -dc > "$BIN"
chmod 700 "$BIN"
"$BIN" "$@"
exit $?
__PAYLOAD_BELOW__
