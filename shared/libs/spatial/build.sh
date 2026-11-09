#!/usr/bin/env bash
# build.sh — compila `libgmbspatial` (namespace SPATIAL: distanze + ConvexHull).
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
bash "$MONO/tools/sh/gmb_lib.sh" build spatial
