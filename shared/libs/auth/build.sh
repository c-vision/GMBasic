#!/usr/bin/env bash
# build.sh — compila `libgmbauth` (JWT HS256 self-contained).
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
bash "$MONO/tools/sh/gmb_lib.sh" build auth
