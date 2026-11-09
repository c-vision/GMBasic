#!/usr/bin/env bash
# build.sh — compila `libgmbsignal` (namespace SIGNAL: finestre, filtri, spettro).
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
bash "$MONO/tools/sh/gmb_lib.sh" build signal
