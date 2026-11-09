#!/usr/bin/env bash
# build.sh — compila `libgmbfinance` (namespace FINANCE: formule finanziarie).
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
bash "$MONO/tools/sh/gmb_lib.sh" build finance
