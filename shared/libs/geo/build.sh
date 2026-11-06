#!/usr/bin/env bash
# build.sh — compila `libgmbgeo` (contratto unico: tools/sh/gmb_lib.sh).
# Namespace BASIC: GEO (geo.inc). Sorgenti gmbgeo (self-contained, solo libm) + shim FFI.
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
bash "$MONO/tools/sh/gmb_lib.sh" build geo
