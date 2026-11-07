#!/usr/bin/env bash
# tui_visual_gallery.sh — giro visivo dei controlli TUI (headless, nessuna finestra).
#
# Costruisce tools/c/tui_visual_gallery.c contro il runtime C e produce due PNG:
#   tools/reports/tui/gallery_tui.png   (buffer a celle + SDLConsole, Turbo Vision)
#   tools/reports/tui/gallery_gfx.png   (schermo grafico, renderer pixel)
#
# Uso:  bash tools/sh/tui_visual_gallery.sh [--dump]
#       --dump  stampa anche la zona TBrowse del buffer su stderr
#
# Non apre finestre (SDL_VIDEODRIVER=dummy): non blocca il desktop.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
GB="$ROOT/GMBasic"
OUT="$ROOT/tools/reports/tui"
BIN="$ROOT/tools/c/bin"
LIBDIR="$HOME/.gmbasic/libs/osx-arm64"

DUMP=""
[ "${1:-}" = "--dump" ] && DUMP="dump"

mkdir -p "$OUT" "$BIN"

RUNTIME="$GB/native/gmb_runtime_c/build/libgmb_runtime.a"
if [ ! -f "$RUNTIME" ]; then
  echo "runtime non compilato: $RUNTIME" >&2
  echo "  cmake --build GMBasic/native/gmb_runtime_c/build --target gmb_runtime" >&2
  exit 1
fi

cc -O1 -w \
  -I "$GB/native/gmb_runtime_c/include" \
  -I "$GB/native/gmb_runtime_c/src" \
  -I "$ROOT/shared/include" \
  -o "$BIN/tui_visual_gallery" \
  "$ROOT/tools/c/tui_visual_gallery.c" \
  "$RUNTIME" "$GB/native/gmb_runtime_c/build/gmb_chart_build/libgmb_chart.a" \
  -L"$LIBDIR" -lSDL2 -lSDL2_ttf -Wl,-rpath,"$LIBDIR" \
  -lpthread -lm -lz -lbz2 -lcurl -lcompression

"$BIN/tui_visual_gallery" tui "$OUT/gallery_tui.png" $DUMP
"$BIN/tui_visual_gallery" gfx "$OUT/gallery_gfx.png"
"$BIN/tui_visual_gallery" window "$OUT/window_gfx.png"

echo "PNG: $OUT/gallery_tui.png"
echo "     $OUT/gallery_gfx.png"
echo "     $OUT/window_gfx.png"
