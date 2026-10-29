#!/usr/bin/env bash
# libs_verify_load.sh — GATE di caricabilità: prova a fare `dlopen` di OGNI libreria della cache
# (`~/.gmbasic/libs/<rid>/`) in un PROCESSO SEPARATO, con il path dinamico puntato alla cache stessa
# — cioè come farà il runtime. Scova la classe di bug "dipendenza mancante dal bundle" (un file
# linkato con `LC_LOAD_DYLIB` non presente → il dlopen fallisce, ma nessun `sha256` se ne accorge).
#
# PERCHÉ ESISTE. Caso reale (2026-10-28): `libgmb_ai_mlx` dipendeva da `libllm_ai_mlx`, che NON era
# né nel manifest né nel bundle → l'engine MLX non partiva, e `libs check` era verde (gli sha dei
# file presenti erano corretti). Gli sha dicono "file integro", non "grafo delle dipendenze completo".
#
# USO
#   tools/sh/libs_verify_load.sh [DIR] [--quiet]
#     DIR      cartella da verificare (default: ~/.gmbasic/libs/<rid della macchina>)
#     --quiet  stampa solo il riepilogo e i fallimenti
#
# EXIT: 0 = tutte caricabili · 1 = almeno una NON caricabile (stampa nome + dipendenza mancante).
#
# NOTA: è uno strumento di sviluppo/CI, non del runtime. Carica le dylib per davvero: alcune
# inizializzano sottosistemi (SDL, torch) — per questo ogni lib gira in un processo figlio isolato.
set -euo pipefail

DIR="${1:-}"; [ "${DIR:-}" = "--quiet" ] && { DIR=""; QUIET=1; }
QUIET="${QUIET:-0}"
[ "${2:-}" = "--quiet" ] && QUIET=1

default_rid() {
  case "$(uname -s)-$(uname -m)" in
    Darwin-arm64) echo "osx-arm64";; Darwin-x86_64) echo "osx-x64";;
    Linux-x86_64) echo "linux-x64";;  Linux-aarch64|Linux-arm64) echo "linux-arm64";;
    MINGW*|MSYS*|CYGWIN*) echo "win-x64";; *) echo "osx-arm64";;
  esac
}
[ -n "$DIR" ] || DIR="$HOME/.gmbasic/libs/$(default_rid)"
[ -d "$DIR" ] || { echo "cartella non trovata: $DIR" >&2; exit 1; }

case "$(uname -s)" in
  Darwin) VAR=DYLD_LIBRARY_PATH ;;
  *)      VAR=LD_LIBRARY_PATH ;;
esac
PY="${PYTHON:-python3}"
command -v "$PY" >/dev/null || { echo "serve python3 (per il dlopen isolato)" >&2; exit 1; }

total=0; bad=0
for f in "$DIR"/*.dylib "$DIR"/*.so "$DIR"/*.so.* "$DIR"/*.dll; do
  [ -f "$f" ] || continue
  base="$(basename "$f")"
  total=$((total+1))
  err="$(env "$VAR=$DIR" "$PY" - "$f" <<'PY' 2>&1
import ctypes, sys
try:
    ctypes.CDLL(sys.argv[1], mode=ctypes.RTLD_GLOBAL)
except OSError as e:
    msg = str(e)
    for line in msg.splitlines():
        if "Library not loaded" in line or "image not found" in line or "cannot open shared" in line:
            print(line.strip()); break
    else:
        print(msg.splitlines()[0] if msg else "errore")
    sys.exit(1)
PY
)" || { bad=$((bad+1)); printf '  ✗ %-34s %s\n' "$base" "$(printf '%s' "$err" | head -1)"; }
  [ "$QUIET" = "1" ] || printf '  ✓ %s\n' "$base"
done

echo "[libs] caricabilità: $((total-bad))/$total OK in $DIR"
[ "$bad" = "0" ] || { echo "[libs] $bad librerie NON caricabili (dipendenza mancante dal bundle?)" >&2; exit 1; }
