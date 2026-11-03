#!/usr/bin/env bash
# build.sh — compila `libgmbopt` (contratto unico: `tools/sh/gmb_lib.sh`).
# Output: shared/libs/opt/libgmbopt.<ext>  ·  export .inc e header in shared/include/.
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
bash "$MONO/tools/sh/gmb_lib.sh" build opt
# I runner compilano con il progetto = cartella del sorgente: copie locali dell'export
# BASIC per gli esempi e i minitest (l'originale resta in shared/include/opt.inc).
cp -f "$MONO/shared/include/opt.inc" "$MONO/shared/samples/include/opt/opt.inc"
cp -f "$MONO/shared/include/opt.inc" "$MONO/shared/minitests/opt/opt.inc"
