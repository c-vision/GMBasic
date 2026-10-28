#!/usr/bin/env bash
# Costruisce il pacchetto webp: shim dylib SELF-CONTAINED (libwebp + libsharpyuv
# staticamente linkate dentro) + header. Prerequisito: webp installato (brew --prefix webp).
# Output in lib/<platform>/libgmbwebp.dylib (un solo file, nessuna lib esterna a runtime).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
WEBP=$(brew --prefix webp 2>/dev/null || true)
[ -n "$WEBP" ] || { echo "webp non trovato (brew install webp)"; exit 1; }
OUT="$HERE/lib/osx-arm64"
mkdir -p "$OUT" "$HERE/include"
rm -f "$OUT/libgmbwebp.dylib"
cc -dynamiclib -O2 -I"$WEBP/include" -I"$HERE/include" "$HERE/src/gmbwebp.c" \
   -o "$OUT/libgmbwebp.dylib" \
   "$WEBP/lib/libwebp.a" "$WEBP/lib/libsharpyuv.a" -lm -lc++
echo "ok: $OUT/libgmbwebp.dylib (self-contained)"
