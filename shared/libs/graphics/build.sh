#!/usr/bin/env bash
# build.sh — compila `libgmbgraphics` (gmbgfx-2d/3d/codec/color/surface + nucleo condiviso).
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
bash "$MONO/tools/sh/gmb_lib.sh" build graphics
