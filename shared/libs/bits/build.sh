#!/usr/bin/env bash
# build.sh — compila `libgmbbits` (namespace BITS: manipolazione bit a 64 bit).
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
bash "$MONO/tools/sh/gmb_lib.sh" build bits
