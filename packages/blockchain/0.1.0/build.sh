#!/usr/bin/env bash
# build.sh — compila il pacchetto `blockchain` (libgmbblockchain.dylib).
# La copia dei sorgenti in src/ e' SELF-CONTAINED: non referenzia gmbsuite ne' GMBasic.
# Unica dipendenza esterna: OpenSSL 3 (libcrypto) — ecco perche' il dominio e' un pacchetto
# e non una shared/libs.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
SSL=$(brew --prefix openssl@3 2>/dev/null || true)
[ -n "$SSL" ] || { echo "openssl@3 non trovato (brew install openssl@3)"; exit 1; }

OUT="$HERE/lib/osx-arm64"
mkdir -p "$OUT" "$HERE/include"
rm -f "$OUT/libgmbblockchain.dylib"
cc -std=c99 -O2 -fPIC -dynamiclib \
   -DHAVE_OPENSSL=1 -DGMB_BC_HAVE_OPENSSL=1 \
   -I"$HERE/include" -I"$SSL/include" \
   "$HERE"/src/*.c \
   -o "$OUT/libgmbblockchain.dylib" \
   -L"$SSL/lib" -lcrypto -lm \
   -install_name "@rpath/libgmbblockchain.dylib"
echo "ok: $OUT/libgmbblockchain.dylib"
