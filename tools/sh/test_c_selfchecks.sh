#!/usr/bin/env bash
# test_c_selfchecks.sh — selfcheck UNITARI in C del runtime (Second Brain, pack firmato).
#
# Build in una cartella DEDICATA (build-tests) per non toccare i build dir di altre sessioni.
# Uso: tools/sh/test_c_selfchecks.sh [--rebuild]
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
# Build FUORI dal repo (il working tree è condiviso: niente cartelle di build tracciate/da
# ripulire). Persistente nella tmp dell'utente tra un run e l'altro.
BUILD="${TMPDIR:-/tmp}/gmb-runtime-c-tests"
SRC="$REPO/GMBasic/native/gmb_runtime_c"
FILTER='^(gmb_brain_registry_selfcheck|gmb_brain_tools_selfcheck|gmb_pkg_pack_signed_selfcheck)$'

if [ ! -f "$BUILD/CMakeCache.txt" ] || [ "${1:-}" = "--rebuild" ]; then
  cmake -S "$SRC" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release >/dev/null || {
    echo "?configurazione fallita ($BUILD)" >&2; exit 1; }
fi

cmake --build "$BUILD" --target gmb_brain_registry_selfcheck gmb_brain_tools_selfcheck gmb_pkg_pack_signed_selfcheck -j4 2>&1 \
  | grep -E "error:|Built target" | tail -5

cd "$BUILD" || exit 1
ctest -R "$FILTER" --output-on-failure
