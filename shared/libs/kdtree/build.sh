#!/usr/bin/env bash
# build.sh — compila `libgmbkdtree` (namespace KDTREE: scipy.spatial.KDTree).
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
bash "$MONO/tools/sh/gmb_lib.sh" build kdtree
