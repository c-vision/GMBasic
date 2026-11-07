#!/usr/bin/env bash
# ui_click_check.sh — test FUNZIONALE dei clic sui controlli (headless, deterministico).
# Costruisce tools/c/ui_click_check.c contro il runtime C e inietta DOWN+UP su ogni
# controllo, verificando che lo stato cambi. exit 0 = tutti PASS.
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
  -o "$BIN/ui_click_check" \
  "$ROOT/tools/c/ui_click_check.c" \
  "$RUNTIME" "$CHART" \
  -L"$LIBDIR" -lSDL2 -lSDL2_ttf -Wl,-rpath,"$LIBDIR" \
  -lpthread -lm -lz -lbz2 -lcurl -lcompression

SDL_VIDEODRIVER=dummy "$BIN/ui_click_check"
