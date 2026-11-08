#!/usr/bin/env bash
# ui_render_audit.sh — GATE: nessun controllo in GRAFICA senza renderer PIXEL.
# Costruisce tools/c/ui_render_audit.c contro il runtime e stampa l'inventario.
# exit 0 = tutti i controlli hanno il renderer pixel.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
GB="$ROOT/GMBasic"
BIN="$ROOT/tools/c/bin"
LIBDIR="$HOME/.gmbasic/libs/osx-arm64"
RUNTIME="$GB/native/gmb_runtime_c/build/libgmb_runtime.a"
CHART="$GB/native/gmb_runtime_c/build/gmb_chart_build/libgmb_chart.a"

mkdir -p "$BIN"
[ -f "$RUNTIME" ] || { echo "runtime non compilato: $RUNTIME" >&2; exit 1; }

cc -O1 -w \
  -I "$GB/native/gmb_runtime_c/include" \
  -I "$GB/native/gmb_runtime_c/src" \
  -o "$BIN/ui_render_audit" \
  "$ROOT/tools/c/ui_render_audit.c" \
  "$RUNTIME" "$CHART" \
  -L"$LIBDIR" -lSDL2 -lSDL2_ttf -Wl,-rpath,"$LIBDIR" \
  -lpthread -lm -lz -lbz2 -lcurl -lcompression

SDL_VIDEODRIVER=dummy "$BIN/ui_render_audit"
