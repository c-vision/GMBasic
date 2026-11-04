#!/usr/bin/env bash
# build.sh — compila `libgmbaviation` (contratto unico: tools/sh/gmb_lib.sh).
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
bash "$MONO/tools/sh/gmb_lib.sh" build aviation
