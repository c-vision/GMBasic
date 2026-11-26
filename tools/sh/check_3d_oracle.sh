#!/usr/bin/env bash
# check_3d_oracle.sh — gate del motore 3D (P1 e P2): oracolo headless.
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
DEMO="$REPO/shared/minitests/3d/worlds/demo.gmbscene"
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

# 1) Gate A/B sui file C toccati (regola di casa: stesso file, cc E tools/sh/zcc).
GFX="$REPO/shared/libs/graphics"
GFXINC="-I $GFX/include -I $REPO/shared/libs/math/include -I $REPO/shared/libs/compute/include"
gate() {   # $1 = sorgente: lo stesso file con cc e con tools/sh/zcc
  local f="$1" k log obj
  for k in cc zcc; do
    log="$BUILD/gate_$k.log"
    obj="$BUILD/${f%.c}_$k.o"
    if [ "$k" = cc ]; then
      cc -O2 -c -I "$SRC/include" $GFXINC "$SRC/src/$f" -o "$obj" 2>"$log"
    else
      bash "$REPO/tools/sh/zcc" -O2 -c -I "$SRC/include" $GFXINC "$SRC/src/$f" -o "$obj" 2>"$log"
    fi || { echo "check_3d_oracle: $f NON compila con $k" >&2; sed -n '1,20p' "$log" >&2; exit 1; }
  done
}
# gmb_3d_scene.c (P5): matrici e culling per oggetto, lo stesso file su tutte le gambe.
for f in gmb_3d_frame.c gmb_3d_io.c gmb_3d_sector.c gmb_3d_move.c gmb_3d_time.c gmb_3d_scene.c gmb_3d_scene_actor.c gmb_3d_scene_buf.c gmb_3d_scene_tex.c; do gate "$f"; done
say "gate C: gmb_3d_{frame,io,sector,move,time,scene,scene_actor,scene_buf,scene_tex}.c compilano con cc e tools/sh/zcc"

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

# 3) Guardie, null mode e load R11: prima di guardare i pixel. I documenti di
# prova del load si scrivono in $BUILD (mai nel repo).
if ! "$BIN" --selftest "$BUILD" --demo "$DEMO"; then
  echo "check_3d_oracle: selftest FALLITO" >&2
  exit 1
fi

# 3bis) Frame time della mappa demo a 640x300: accettazione P2 (<= 4 ms).
if ! "$BIN" --bench --demo "$DEMO" --frames 120; then
  echo "check_3d_oracle: la mappa demo supera il limite di frame time" >&2
  exit 1
fi

# 3ter) P5: culling per oggetto (M-CULL). Conteggi simmetrici e — soprattutto —
# hash IDENTICO fra una scena con gli oggetti fuori campo e la stessa senza: il
# culling salta lavoro, non cambia un pixel (le golden restano valide).
if ! "$BIN" --cullcheck; then
  echo "check_3d_oracle: culling per oggetto FALLITO" >&2
  exit 1
fi

# 3ter-bis) P5: superficie interattiva (M-PLAYER/M-COLLIDE/M-FLOOR/M-ZONE).
# Corpo, MOVE con scivolamento/gradini, HIT, quote sovrapposte, zone e la
# PRIMA che vince: tutto headless, stessi numeri su tutte le gambe.
if ! "$BIN" --interactcheck; then
  echo "check_3d_oracle: superficie interattiva FALLITA" >&2
  exit 1
fi

# 3quater) P5: banco della scena. 500 oggetti a 640x360, accettazione >= 30 fps
# (mediana <= 33,3 ms). Il numero stampato e' quello misurato, non una stima.
if ! P5_OUT="$("$BIN" --bench-scene --objects 500 --size 640x360 --frames 60)"; then
  echo "check_3d_oracle: banco della scena fallito" >&2
  exit 1
fi
say "$P5_OUT"
P5_MED="$(printf '%s\n' "$P5_OUT" | sed -n 's/.* med=\([0-9.][0-9.]*\).*/\1/p')"
if [ -z "$P5_MED" ]; then
  echo "check_3d_oracle: banco della scena senza mediana" >&2
  exit 1
fi
if awk "BEGIN{exit !($P5_MED > 33.34)}"; then
  echo "check_3d_oracle: 500 oggetti a 640x360 sotto 30 fps (med=${P5_MED} ms)" >&2
  exit 1
fi

# 3quater-bis) P5: accettazione dei 200k triangoli (capacità dinamica dei buffer).
# 200.000 triangoli dichiarati (16.667 box da 12) a 640x360, >= 30 fps. Il banco
# FALLISCE da solo se un frame alloca (allocs != 0), quindi il check copre anche
# «nessuna allocazione per frame».
if ! P5T_OUT="$("$BIN" --bench-scene --tris 200000 --size 640x360 --frames 20)"; then
  echo "check_3d_oracle: banco dei 200k triangoli fallito (o frame con allocazioni)" >&2
  exit 1
fi
say "$P5T_OUT"
P5T_MED="$(printf '%s\n' "$P5T_OUT" | sed -n 's/.* med=\([0-9.][0-9.]*\).*/\1/p')"
if [ -z "$P5T_MED" ]; then
  echo "check_3d_oracle: banco dei 200k senza mediana" >&2
  exit 1
fi
if awk "BEGIN{exit !($P5T_MED > 33.34)}"; then
  echo "check_3d_oracle: 200k triangoli a 640x360 sotto 30 fps (med=${P5T_MED} ms)" >&2
  exit 1
fi

# 4) Golden. Il RID lo decide il binario (GMB_ORACLE_RID, compilato all'origine):
# nessuna seconda derivazione con uname da tenere allineata a mano.
if [ "$BLESS" = "1" ]; then
  mkdir -p "$GOLDEN" || exit 1
  "$BIN" --bless "$GOLDEN" --demo "$DEMO"
  exit $?
fi
if [ ! -d "$GOLDEN" ]; then
  echo "GOLDEN SKIP: manca la cartella $GOLDEN (generala con --bless)" >&2
  exit 0
fi
"$BIN" --check "$GOLDEN" --tol "$TOL" --demo "$DEMO"
