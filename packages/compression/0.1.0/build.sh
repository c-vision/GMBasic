#!/usr/bin/env bash
# build.sh — compila il pacchetto `compression` (libgmbcompression).
#
# Sorgenti self-contained in src/ (copia dei kernel del runtime con prefisso
# gmbcp_): nessun riferimento a gmbsuite o a GMBasic. Dipendenze esterne:
# zlib, zstd, lz4, xz/lzma, bzip2, brotli — ecco perche' il dominio e' un
# pacchetto e non una shared/libs.
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

INC=""
LIBP=""
if command -v brew >/dev/null 2>&1; then
  for p in zstd lz4 xz brotli bzip2; do
    P=$(brew --prefix "$p" 2>/dev/null || true)
    [ -n "$P" ] && INC="$INC -I$P/include" && LIBP="$LIBP -L$P/lib"
  done
fi

LIBS="-lz -lzstd -llz4 -llzma -lbz2 -lbrotlienc -lbrotlidec -lbrotlicommon -lm"

if [ "$UNAME" = "Darwin" ]; then
  rm -f "$OUT/libgmbcompression.dylib"
  cc -std=c99 -O2 -fPIC -dynamiclib \
     -I"$HERE/include" $INC \
     "$HERE"/src/*.c \
     $LIBP $LIBS \
     -o "$OUT/libgmbcompression.dylib" \
     -install_name "@rpath/libgmbcompression.dylib"
  echo "ok: $OUT/libgmbcompression.dylib"
else
  rm -f "$OUT/libgmbcompression.so"
  cc -std=c99 -O2 -fPIC -shared \
     -I"$HERE/include" $INC \
     "$HERE"/src/*.c \
     $LIBP $LIBS \
     -o "$OUT/libgmbcompression.so"
  echo "ok: $OUT/libgmbcompression.so"
fi
