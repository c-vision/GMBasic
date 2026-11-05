#!/usr/bin/env bash
# build.sh — compila `libgmbgraphics` (gmbgraphics: colore + geometria 2D; i domini successivi
# aggiungono sorgenti senza cambiare il contratto).
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
bash "$MONO/tools/sh/gmb_lib.sh" build graphics
