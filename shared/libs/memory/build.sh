#!/usr/bin/env bash
# build.sh — compila `libgmbmemory` (MEMORY.* su SQLite in-memory).
# SQLite è una libreria di piattaforma/supporto GMBasic (macOS: /usr/lib; Linux:
# gruppo `support` del libs-manifest), non una dipendenza da installare.
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
bash "$MONO/tools/sh/gmb_lib.sh" build memory
