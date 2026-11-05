#!/usr/bin/env bash
# build.sh — compila `libgmbpgm` (contratto unico: tools/sh/gmb_lib.sh).
# Namespace BASIC: PGM (pgm.inc). Motore: gmbortools `gor_pgm` (AppendPGM, MIT)
# + shim ABI a handle in src/pgm_ffi.c. Sources copiati da gmbsuite/gmbortools.
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
bash "$MONO/tools/sh/gmb_lib.sh" build pgm
