#!/usr/bin/env bash
# ui_tui_parity.sh — PROVA VISIVA della parità TUI/GUI: crea i controlli con le
# factory TUI dentro una pagina grafica e salva tools/reports/ui_tui_parity.png.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
GB="$ROOT/GMBasic"; BIN="$ROOT/tools/c/bin"; LIBDIR="$HOME/.gmbasic/libs/osx-arm64"
RUNTIME="$GB/native/gmb_runtime_c/build/libgmb_runtime.a"
CHART="$GB/native/gmb_runtime_c/build/gmb_chart_build/libgmb_chart.a"
mkdir -p "$BIN" "$ROOT/tools/reports"
[ -f "$RUNTIME" ] || { echo "runtime non compilato" >&2; exit 1; }
cc -O1 -w -I "$GB/native/gmb_runtime_c/include" -I "$GB/native/gmb_runtime_c/src" \
  -o "$BIN/ui_tui_parity" "$ROOT/tools/c/ui_tui_parity.c" "$RUNTIME" "$CHART" \
  -L"$LIBDIR" -lSDL2 -lSDL2_ttf -Wl,-rpath,"$LIBDIR" -lpthread -lm -lz -lbz2 -lcurl -lcompression
cd "$ROOT"
SDL_VIDEODRIVER=dummy "$BIN/ui_tui_parity"
echo "immagine: tools/reports/ui_tui_parity.png"
