#!/usr/bin/env bash
# tools/sh/test_c99_first.sh — PIPELINE DI TEST "C PRIMA" (direttiva 2026-11-19).
#
# ORDINE INDEROGABILE:
#   1. GATE DI EMISSIONE  — ogni `.bas` del corpus (o il sottoinsieme scelto) deve essere
#                           emettibile dal backend C99Native (`gmbnative --no-exe`);
#   2. RUN NATIVO         — i MiniTest girano sul C99 (gamba di riferimento);
#   3. PARITÀ (opzionale) — solo dopo: IL/PCode/VM-C confrontati col C99.
#
# PERCHÉ. Un sample che il C non riesce nemmeno a emettere non è "non testato": è rotto, e
# finché il suo unico test compila su IL il buco resta invisibile (misurati 33/3659 in questa
# forma, poi 4 dopo le conversioni). Con questo ordine il C non può più essere verde per caso.
#
# USO
#   tools/sh/test_c99_first.sh                    # gate su tutto + MiniTest nativi
#   tools/sh/test_c99_first.sh --only matrix/     # gate sui file che contengono "matrix/" + run
#   tools/sh/test_c99_first.sh --parity           # + confronto 4 gambe (più lento)
#   tools/sh/test_c99_first.sh --samples          # gate sui sample (non solo minitest)
#   tools/sh/test_c99_first.sh --jobs 8
#
# NB su `--only`: il GATE filtra per COMPONENTE di path ("io/" prende solo minitests/io/…),
# il runner nativo per SOTTOSTRINGA (può eseguire qualche file in più: innocuo, sono comunque
# MiniTest sulla gamba C).
#
# USCITA: 0 = tutte le fasi verdi; 1 = fallita una fase (il messaggio dice quale).
set -u
DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$DIR/../.." && pwd)"
cd "$ROOT" || exit 2

ONLY=""; JOBS=""; PARITY=0; SCOPE="--minitests"
while [ $# -gt 0 ]; do
  case "$1" in
    --only)    ONLY="$2"; shift 2 ;;
    --jobs)    JOBS="$2"; shift 2 ;;
    --parity)  PARITY=1; shift ;;
    --samples) SCOPE="--samples"; shift ;;
    --all)     SCOPE=""; shift ;;
    -h|--help) sed -n '2,26p' "$0"; exit 0 ;;
    *) echo "opzione sconosciuta: $1" >&2; exit 2 ;;
  esac
done

say() { printf '\n\033[1m== %s ==\033[0m\n' "$1"; }

say "FASE 1/3 — gate di emissione C99"
GATE_ARGS=()
[ -n "$SCOPE" ] && GATE_ARGS+=("$SCOPE")
[ -n "$ONLY" ] && GATE_ARGS+=(--only "$ONLY")
[ -n "$JOBS" ] && GATE_ARGS+=(--jobs "$JOBS")
if ! bash "$DIR/c99_emit_gate.sh" "${GATE_ARGS[@]+"${GATE_ARGS[@]}"}"; then
  echo "" >&2
  echo "STOP: il backend C non emette tutto il corpus — il resto della pipeline non ha senso." >&2
  echo "Ripara l'emissione (o converti il sample) e rilancia." >&2
  exit 1
fi

say "FASE 2/3 — MiniTest sulla gamba nativa (C99)"
if [ -n "$ONLY" ]; then
  GMB_MINI_ONLY="$ONLY" bash "$DIR/test_minitests_c99native" || { echo "STOP: MiniTest nativi rossi." >&2; exit 1; }
else
  bash "$DIR/test_minitests_c99native" || { echo "STOP: MiniTest nativi rossi." >&2; exit 1; }
fi

if [ "$PARITY" -eq 1 ]; then
  say "FASE 3/3 — parità 4 gambe (C99 è il riferimento)"
  if [ -n "$ONLY" ]; then
    zsh "$DIR/test_minitests_parity.sh" "$ONLY" || { echo "STOP: divergenza tra gambe." >&2; exit 1; }
  else
    zsh "$DIR/test_minitests_parity.sh" "" || { echo "STOP: divergenza tra gambe." >&2; exit 1; }
  fi
else
  say "FASE 3/3 — parità: SALTATA (aggiungi --parity per eseguirla)"
fi

say "C-FIRST: tutte le fasi verdi"
exit 0
