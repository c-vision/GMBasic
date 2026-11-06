#!/usr/bin/env bash
# build.sh — compila libffi 3.5.2 (vendored) come STATICA in out/.
# Sorgenti nel repo: nessuna dipendenza di sistema. Richiede sh/make/cc (macOS/Linux; su
# Windows usare msvc_build/ o una lib precompilata).
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$HERE/out"
if [ -f "$OUT/lib/libffi.a" ]; then
  echo "[ffi] già compilata: $OUT/lib/libffi.a"
  exit 0
fi
cd "$HERE"
./configure --disable-shared --enable-static --disable-docs --prefix="$OUT" >/dev/null
make -j"${JOBS:-6}" >/dev/null
make install >/dev/null
echo "[ffi] $OUT/lib/libffi.a"
