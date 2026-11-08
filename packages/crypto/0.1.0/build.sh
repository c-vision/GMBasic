#!/usr/bin/env bash
# build.sh — compila il pacchetto `crypto` (libgmbcrypto.dylib).
# La copia dei sorgenti in src/ e' SELF-CONTAINED: non referenzia gmbsuite ne' GMBasic.
# Unica dipendenza esterna: OpenSSL 3 (libcrypto) — ecco perche' il dominio e' un pacchetto
# e non una lib di shared/libs.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
SSL=$(brew --prefix openssl@3 2>/dev/null || true)
[ -n "$SSL" ] || { echo "openssl@3 non trovato (brew install openssl@3)"; exit 1; }

OUT="$HERE/lib/osx-arm64"
mkdir -p "$OUT" "$HERE/include"
rm -f "$OUT/libgmbcrypto.dylib"
cc -std=c99 -O2 -fPIC -DHAVE_OPENSSL=1 -dynamiclib \
   -I"$HERE/include" -I"$SSL/include" \
   "$HERE"/src/*.c \
   -o "$OUT/libgmbcrypto.dylib" \
   -L"$SSL/lib" -lcrypto -lm \
   -install_name "@rpath/libgmbcrypto.dylib"
echo "ok: $OUT/libgmbcrypto.dylib"
