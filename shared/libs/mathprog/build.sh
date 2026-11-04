#!/usr/bin/env bash
# build.sh — compila `libgmbmathprog` (contratto unico: tools/sh/gmb_lib.sh).
# Namespace BASIC: OPTIMIZE (mathprog.inc). Motore: gmbortools (gor_lp, gor_model, simplex).
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
bash "$MONO/tools/sh/gmb_lib.sh" build mathprog
