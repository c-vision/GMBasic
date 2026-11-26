#!/usr/bin/env bash
# bench_3d.sh — banco misurato del motore 3D (P5 del piano, §9 punto 6).
#
# Misura il tempo MEDIANO per frame della superficie SCENE (non una stima) su tre
# risoluzioni. Il numero che conta è quello stampato dal binario; la macchina è
# condivisa, quindi si riporta anche il minimo (§0septies).
#
# Uso: tools/sh/bench_3d.sh                        # 500 oggetti (6000 triangoli)
#      tools/sh/bench_3d.sh 500 60                 # forma posizionale storica
#      tools/sh/bench_3d.sh --tris 200000          # accettazione P5: ~200k triangoli
#      tools/sh/bench_3d.sh --tris 200000 --frames 20 --res 640x360
#
#   --objects N   N oggetti in griglia 25 colonne davanti alla camera (storico)
#   --tris N      N triangoli dichiarati (12 per box) in griglia quadrata frontale;
#                 serve a misurare i 200k dell'accettazione P5 senza uscire dal frustum
#   --frames F    frame misurati per risoluzione (default 60; 20 in modalità --tris)
#   --res LISTA   risoluzioni separate da virgola (default 320x180,640x360,960x540)
#
# Il binario è l'oracolo headless (tools/sh/check_3d_oracle.sh), costruito in una
# cartella FUORI dal repo: nessuna cartella di build da ripulire nel working tree.
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$REPO/GMBasic/native/gmb_runtime_c"
BUILD="${TMPDIR:-/tmp}/gmb-3d-oracle-build"

MODE="objects"
VALUE=500
FRAMES=""
RESLIST="320x180,640x360,960x540"

# Forma posizionale storica: `bench_3d.sh [N] [FRAMES]`.
if [ $# -gt 0 ] && [ "${1#-}" = "$1" ]; then
  VALUE="$1"; shift
  if [ $# -gt 0 ] && [ "${1#-}" = "$1" ]; then FRAMES="$1"; shift; fi
fi
while [ $# -gt 0 ]; do
  case "$1" in
    --objects) MODE="objects"; VALUE="${2:-}"; shift 2 ;;
    --tris)    MODE="tris";    VALUE="${2:-}"; shift 2 ;;
    --frames)  FRAMES="${2:-}"; shift 2 ;;
    --res)     RESLIST="${2:-}"; shift 2 ;;
    -h|--help) sed -n '2,24p' "$0"; exit 0 ;;
    *) echo "bench_3d: argomento non riconosciuto: $1" >&2; exit 2 ;;
  esac
done
[ -n "$FRAMES" ] || { if [ "$MODE" = "tris" ]; then FRAMES=20; else FRAMES=60; fi; }
case "$VALUE" in ''|*[!0-9]*) echo "bench_3d: valore non numerico: '$VALUE'" >&2; exit 2 ;; esac

mkdir -p "$BUILD" || exit 1
if [ ! -f "$BUILD/CMakeCache.txt" ]; then
  cmake -S "$SRC" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release >"$BUILD/configure.log" 2>&1 || {
    echo "bench_3d: configurazione fallita (vedi $BUILD/configure.log)" >&2; exit 1; }
fi
if ! cmake --build "$BUILD" --target gmb_3d_oracle -j4 >"$BUILD/build.log" 2>&1; then
  echo "bench_3d: build fallita (vedi $BUILD/build.log)" >&2
  grep -E "error:|Error [0-9]" "$BUILD/build.log" | head -20 >&2
  exit 1
fi
BIN="$BUILD/gmb_3d_oracle"
[ -x "$BIN" ] || { echo "bench_3d: binario non trovato: $BIN" >&2; exit 1; }

if [ "$MODE" = "tris" ]; then
  echo "banco 3D — $VALUE triangoli (12 per box) in griglia frontale, $FRAMES frame per risoluzione"
else
  echo "banco 3D — $VALUE oggetti, $FRAMES frame per risoluzione"
fi
printf '%-12s %8s %8s %8s %8s %8s %8s\n' "risoluzione" "tris" "drawn" "culled" "min_ms" "med_ms" "fps_med"

IFS=',' read -r -a SIZES <<< "$RESLIST"
for size in "${SIZES[@]}"; do
  if [ "$MODE" = "tris" ]; then
    out="$("$BIN" --bench-scene --tris "$VALUE" --size "$size" --frames "$FRAMES")" || {
      echo "bench_3d: $size fallita" >&2; exit 1; }
  else
    out="$("$BIN" --bench-scene --objects "$VALUE" --size "$size" --frames "$FRAMES")" || {
      echo "bench_3d: $size fallita" >&2; exit 1; }
  fi
  tris="$(printf '%s\n' "$out" | sed -n 's/.* tris=\([0-9]*\).*/\1/p')"
  drawn="$(printf '%s\n' "$out" | sed -n 's/.* drawn=\([0-9]*\).*/\1/p')"
  culled="$(printf '%s\n' "$out" | sed -n 's/.* culled=\([0-9]*\).*/\1/p')"
  minm="$(printf '%s\n' "$out" | sed -n 's/.* min=\([0-9.][0-9.]*\).*/\1/p')"
  med="$(printf '%s\n' "$out" | sed -n 's/.* med=\([0-9.][0-9.]*\).*/\1/p')"
  fps="$(awk "BEGIN{ if ($med > 0) printf \"%.1f\", 1000.0 / $med; else printf \"-\" }")"
  printf '%-12s %8s %8s %8s %8s %8s %8s\n' "$size" "$tris" "$drawn" "$culled" "$minm" "$med" "$fps"
done
