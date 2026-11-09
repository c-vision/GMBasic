#!/usr/bin/env bash
# build.sh — compila il pacchetto `signal` (libgmbsignal).
#
# Sorgenti self-contained in src/ (copia dei canonici di gmbsuite/gmbsignal):
# nessun riferimento a gmbsuite o a GMBasic. Unica dipendenza: libm — il
# dominio e' puro calcolo, ecco perche' non serve nessuna libreria esterna.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
UNAME=$(uname -s)
ARCH=$(uname -m)
case "$UNAME-$ARCH" in
  Darwin-arm64)  RID=osx-arm64 ;;
  Darwin-x86_64) RID=osx-x64 ;;
  Linux-x86_64)  RID=linux-x64 ;;
  Linux-aarch64) RID=linux-arm64 ;;
  *)             RID="$UNAME-$ARCH" ;;
esac

OUT="$HERE/lib/$RID"
mkdir -p "$OUT" "$HERE/include"

if [ "$UNAME" = "Darwin" ]; then
  rm -f "$OUT/libgmbsignal.dylib"
  cc -std=c99 -O2 -fPIC -dynamiclib \
     -I"$HERE/include" \
     "$HERE"/src/*.c \
     -lm \
     -o "$OUT/libgmbsignal.dylib" \
     -install_name "@rpath/libgmbsignal.dylib"
  echo "ok: $OUT/libgmbsignal.dylib"
else
  rm -f "$OUT/libgmbsignal.so"
  cc -std=c99 -O2 -fPIC -shared \
     -I"$HERE/include" \
     "$HERE"/src/*.c \
     -lm \
     -o "$OUT/libgmbsignal.so"
  echo "ok: $OUT/libgmbsignal.so"
fi
