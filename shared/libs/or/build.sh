#!/usr/bin/env bash
# build.sh — compila `libgmbor` (contratto unico: tools/sh/gmb_lib.sh).
# Namespace BASIC: OPTIMIZE (ortools.inc). Motore: gmbortools (pack/assign/cover/cp).
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
bash "$MONO/tools/sh/gmb_lib.sh" build or
