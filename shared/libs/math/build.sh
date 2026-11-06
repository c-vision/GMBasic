#!/usr/bin/env bash
# build.sh — compila `libgmbmath` (gmbmath-* + gmbcompute condiviso via shared-src.txt).
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
bash "$MONO/tools/sh/gmb_lib.sh" build math
