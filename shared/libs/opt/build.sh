#!/usr/bin/env bash
# build.sh — compila `libgmbopt` (contratto unico: `tools/sh/gmb_lib.sh`).
# Output: shared/libs/opt/libgmbopt.<ext>  ·  export .inc e header in shared/include/.
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
bash "$MONO/tools/sh/gmb_lib.sh" build opt
# I runner compilano con il progetto = cartella del sorgente: copia locale dell'export
# BASIC solo per i MiniTest (l'originale resta in shared/include/opt.inc).
# NB: NIENTE copia sotto shared/samples/include/ — il loader risolve l'inc canonico e una
# seconda copia faceva "simbolo ... già dichiarato" (2026-11-03).
cp -f "$MONO/shared/include/opt.inc" "$MONO/shared/minitests/opt/opt.inc"
