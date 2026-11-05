#!/usr/bin/env bash
# build.sh — compila `libgmbmath` (gmbcompute + core + special + linalg + numerical + vector).
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
bash "$MONO/tools/sh/gmb_lib.sh" build math
