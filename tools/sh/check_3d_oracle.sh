#!/usr/bin/env bash
# check_3d_oracle.sh — gate P1 (piano 3D): oracolo headless.
#
# Pipeline: raster della suite -> TARGET 3D del runtime -> cattura DAL TARGET -> PNG
# -> golden per RID. Non usa gmb_gfx_present_capture (headless = -1).
#
# Uso: tools/sh/check_3d_oracle.sh [--bless] [--tol PCT] [--quiet]
#
# Build in una cartella FUORI dal repo (come tools/sh/test_c_selfchecks.sh): il working
# tree è condiviso e non si creano cartelle di build da ripulire.
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$REPO/GMBasic/native/gmb_runtime_c"
GOLDEN="$REPO/shared/minitests/3d/golden"
BUILD="${TMPDIR:-/tmp}/gmb-3d-oracle-build"

BLESS=0
TOL="0.0"
QUIET=0
while [ $# -gt 0 ]; do
  case "$1" in
    --bless) BLESS=1; shift ;;
    --tol)   TOL="${2:-0.0}"; shift 2 ;;
    --quiet) QUIET=1; shift ;;
    -h|--help) sed -n '2,10p' "$0"; exit 0 ;;
    *) echo "check_3d_oracle: argomento non riconosciuto: $1" >&2; exit 2 ;;
  esac
done
say() { [ "$QUIET" = "1" ] || echo "$@"; }

mkdir -p "$BUILD" || exit 1

# 1) Gate A/B sul file C toccato (regola di casa: stesso file, cc E tools/sh/zcc).
if ! cc -O2 -c -I "$SRC/include" "$SRC/src/gmb_3d_frame.c" -o "$BUILD/frame_cc.o" 2>"$BUILD/gate_cc.log"; then
  echo "check_3d_oracle: gmb_3d_frame.c NON compila con cc" >&2
  sed -n '1,20p' "$BUILD/gate_cc.log" >&2
  exit 1
fi
if ! bash "$REPO/tools/sh/zcc" -O2 -c -I "$SRC/include" "$SRC/src/gmb_3d_frame.c" -o "$BUILD/frame_zcc.o" 2>"$BUILD/gate_zcc.log"; then
  echo "check_3d_oracle: gmb_3d_frame.c NON compila con tools/sh/zcc" >&2
  sed -n '1,20p' "$BUILD/gate_zcc.log" >&2
  exit 1
fi
say "gate C: gmb_3d_frame.c compila con cc e tools/sh/zcc"

# 2) Build dell'oracolo (target dedicato; il runtime statico è una sua dipendenza).
if [ ! -f "$BUILD/CMakeCache.txt" ]; then
  say "configuro $BUILD"
  if ! cmake -S "$SRC" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release >"$BUILD/configure.log" 2>&1; then
    echo "check_3d_oracle: configurazione fallita (vedi $BUILD/configure.log)" >&2
    grep -E "Error|error:" "$BUILD/configure.log" | head -10 >&2
    exit 1
  fi
fi
if ! cmake --build "$BUILD" --target gmb_3d_oracle -j4 >"$BUILD/build.log" 2>&1; then
  echo "check_3d_oracle: build fallita (vedi $BUILD/build.log)" >&2
  grep -E "error:|Error [0-9]" "$BUILD/build.log" | head -20 >&2
  exit 1
fi
BIN="$BUILD/gmb_3d_oracle"
[ -x "$BIN" ] || { echo "check_3d_oracle: binario non trovato: $BIN" >&2; exit 1; }

# 3) Guardie e null mode: prima di guardare i pixel.
if ! "$BIN" --selftest; then
  echo "check_3d_oracle: selftest FALLITO" >&2
  exit 1
fi

# 4) Golden. Il RID lo decide il binario (GMB_ORACLE_RID, compilato all'origine):
# nessuna seconda derivazione con uname da tenere allineata a mano.
if [ "$BLESS" = "1" ]; then
  mkdir -p "$GOLDEN" || exit 1
  "$BIN" --bless "$GOLDEN"
  exit $?
fi
if [ ! -d "$GOLDEN" ]; then
  echo "GOLDEN SKIP: manca la cartella $GOLDEN (generala con --bless)" >&2
  exit 0
fi
"$BIN" --check "$GOLDEN" --tol "$TOL"
