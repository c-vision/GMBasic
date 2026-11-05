#!/usr/bin/env bash
# ==============================================================================
# no_python_guard.sh — impedisce meccanicamente l'uso di Python nel monorepo.
#
# Regola del progetto: gli strumenti si scrivono in BASIC (tools/basic), non in
# Python. Questo script installa un SORVEGLIANTE (tools/c/pyguard.c, FSEvents su
# macOS) che appena un file *.py compare nell'albero lo sposta in quarantena
# (~/.gmbasic/quarantine/py/<stamp>/<nome>) e lo registra in /tmp/gmb_pyguard.log.
#
# Esclusi (sorgenti di terzi, non nostri strumenti):
#   build/  _deps/  .build/  .content/  node_modules/  .git/  vendor/
#
# NOTA macOS: il plist lancia il BINARIO (non lo script) perche' launchd non ha i
# permessi TCC per eseguire script dentro ~/Documents ("Operation not permitted").
# `install` ricompila sempre il binario prima di registrare il plist.
#
# USO
#   tools/sh/no_python_guard.sh scan        una passata (quarantena dei .py presenti)
#   tools/sh/no_python_guard.sh run         sorvegliante in primo piano
#   tools/sh/no_python_guard.sh install     avvia il sorvegliante al login (launchd)
#   tools/sh/no_python_guard.sh uninstall   ferma e rimuove il sorvegliante
#   tools/sh/no_python_guard.sh status      stato del sorvegliante
#   tools/sh/no_python_guard.sh install-hook  hook pre-commit che rifiuta i .py
# ==============================================================================
set -eu

MONO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC="$MONO/tools/c/pyguard.c"
BIN="$MONO/tools/c/bin/pyguard"
QUAR="$HOME/.gmbasic/quarantine/py"
LOG="$HOME/.gmbasic/logs/pyguard.log"
LABEL="com.gmbasic.nopython"
PLIST="$HOME/Library/LaunchAgents/$LABEL.plist"

build() {
  mkdir -p "$(dirname "$BIN")"
  case "$(uname -s)" in
    Darwin) cc -O2 -o "$BIN" "$SRC" -framework CoreServices ;;
    *)      cc -O2 -o "$BIN" "$SRC" ;;
  esac
}

cmd_scan() {
  build
  mkdir -p "$QUAR"
  "$BIN" --scan "$MONO" "$QUAR"
}

cmd_run() {
  build
  mkdir -p "$QUAR" "$(dirname "$LOG")"
  exec "$BIN" "$MONO" "$QUAR"
}

cmd_install() {
  build
  mkdir -p "$QUAR" "$(dirname "$LOG")" "$(dirname "$PLIST")"
  cat > "$PLIST" <<PLISTEOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key><string>$LABEL</string>
  <key>ProgramArguments</key>
  <array>
    <string>$BIN</string>
    <string>$MONO</string>
    <string>$QUAR</string>
  </array>
  <key>RunAtLoad</key><true/>
  <key>KeepAlive</key><true/>
  <key>StandardOutPath</key><string>$LOG</string>
  <key>StandardErrorPath</key><string>$LOG</string>
  <key>WorkingDirectory</key><string>$MONO</string>
</dict>
</plist>
PLISTEOF
  launchctl unload "$PLIST" 2>/dev/null || true
  launchctl load "$PLIST"
  echo "installato: $PLIST (log: $LOG)"
}

cmd_uninstall() {
  if [ -f "$PLIST" ]; then
    launchctl unload "$PLIST" 2>/dev/null || true
    rm -f "$PLIST"
    echo "rimosso: $PLIST"
  else
    echo "non installato"
  fi
}

cmd_status() {
  if [ -f "$PLIST" ]; then
    echo "plist: presente ($PLIST)"
    launchctl list | grep -F "$LABEL" || echo "  processo: non in esecuzione"
  else
    echo "plist: assente (sorvegliante non installato)"
  fi
  if pgrep -f "$BIN" >/dev/null 2>&1; then
    echo "processo: attivo"
  else
    echo "processo: non attivo"
  fi
  echo "quarantena: $(find "$QUAR" -type f 2>/dev/null | wc -l | tr -d ' ') file in $QUAR"
}

cmd_install_hook() {
  local hook="$MONO/.git/hooks/pre-commit"
  if [ -f "$hook" ] && ! grep -q "no_python_guard" "$hook" 2>/dev/null; then
    echo "ATTENZIONE: esiste gia' un pre-commit senza no_python_guard: non lo tocco." >&2
    echo "Aggiungi a mano questa riga:  tools/sh/no_python_guard.sh check-staged" >&2
    return 1
  fi
  cat > "$hook" <<'HOOKEOF'
#!/usr/bin/env bash
# no_python_guard — rifiuta i file .py nel commit (gli strumenti si scrivono in BASIC).
files=$(git diff --cached --name-only --diff-filter=ACMR | grep -E '\.py$' || true)
if [ -n "$files" ]; then
  echo "ERRORE: file Python nel commit (vietato: usare tools/basic/*.bas):" >&2
  echo "$files" | sed 's/^/  /' >&2
  exit 1
fi
HOOKEOF
  chmod +x "$hook"
  echo "installato: $hook"
}

cmd_check_staged() {
  files=$(git -C "$MONO" diff --cached --name-only --diff-filter=ACMR | grep -E '\.py$' || true)
  if [ -n "$files" ]; then
    echo "ERRORE: file Python in stage:" >&2
    echo "$files" | sed 's/^/  /' >&2
    return 1
  fi
  return 0
}

case "${1:-}" in
  scan)          cmd_scan ;;
  run)           cmd_run ;;
  install)       cmd_install ;;
  uninstall)     cmd_uninstall ;;
  status)        cmd_status ;;
  install-hook)  cmd_install_hook ;;
  check-staged)  cmd_check_staged ;;
  *)             sed -n '2,24p' "$0"; exit 2 ;;
esac
