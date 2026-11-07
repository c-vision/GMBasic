#!/usr/bin/env bash
# build.sh — compila `libgmbquant` (gmbquant: general, metrics, volatility, fundamentals).
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
bash "$MONO/tools/sh/gmb_lib.sh" build quant
