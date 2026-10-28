#!/usr/bin/env bash
# Costruisce lo shim del pacchetto mongo (nessuna dipendenza esterna).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
OUT="$HERE/lib/osx-arm64"
mkdir -p "$OUT"
cc -dynamiclib -O2 -I"$HERE/include" "$HERE/src/gmbmongo.c" -o "$OUT/libgmbmongo.dylib" -lpthread
echo "ok: $OUT/libgmbmongo.dylib"
