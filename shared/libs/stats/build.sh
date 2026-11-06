#!/usr/bin/env bash
# build.sh — compila `libgmbstats` (gmbstats-*) con gmbmath/gmbcompute condivisi via shared-src.txt.
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
bash "$MONO/tools/sh/gmb_lib.sh" build stats
