#!/usr/bin/env bash
# Costruisce lo shim dom su libxml2 (brew: $(brew --prefix libxml2)).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
LX=$(brew --prefix libxml2)
[ -d "$LX" ] || { echo "libxml2 non trovata (brew install libxml2)"; exit 1; }
OUT="$HERE/lib/osx-arm64"; mkdir -p "$OUT"
cc -dynamiclib -O2 -I"$LX/include/libxml2" -I"$HERE/include" "$HERE/src/gmbdom.c" \
   -o "$OUT/libgmbdom.dylib" -L"$LX/lib" -lxml2 -Wl,-rpath,"$LX/lib"
echo "ok: $OUT/libgmbdom.dylib"
