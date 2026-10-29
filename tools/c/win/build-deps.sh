#!/usr/bin/env bash
# build-deps.sh — costruisce le TERZE PARTI per Windows ARM64 con Zig (nessun MSVC, nessun admin).
#
# PERCHÉ. Il runtime C dipende da librerie esterne: senza di loro i file che le usano non compilano
# (`gmb_compress`, `gmb_parquet_vtab`, i vtab SQL…) e il link della DLL resta bloccato da simboli
# indefiniti. Qui si costruiscono per `aarch64-windows-gnu` in `<out>/include` e `<out>/lib`.
#
# USO:
#   tools/c/win/build-deps.sh [--out DIR] [--zig PATH] [zlib sqlite3 zstd lz4 lzma brotli pcre2]
#
# Senza argomenti costruisce quelle già supportate (zlib, sqlite3). Le altre sono predisposte ma
# richiedono i rispettivi sorgenti: meglio aggiungerle una alla volta e verificare che il runtime
# compili (vedi tools/c/win/README.md).
set -eu

OUT="$HOME/gmb-winbuild/deps/win-arm64"
ZIG_BIN="${ZIG:-$HOME/ai/tools/zig-aarch64-macos-0.17.0/zig}"
DEPS=""
while [ $# -gt 0 ]; do
  case "$1" in
    --out) OUT="$2"; shift 2 ;;
    --zig) ZIG_BIN="$2"; shift 2 ;;
    *)     DEPS="$DEPS $1"; shift ;;
  esac
done
[ -n "$DEPS" ] || DEPS="zlib sqlite3"
[ -x "$ZIG_BIN" ] || { echo "zig non trovato: $ZIG_BIN (--zig PATH)" >&2; exit 2; }

WORK="$(mktemp -d "${TMPDIR:-/tmp}/gmbwindeps.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$OUT/include" "$OUT/lib"

fetch() { # url destinazione
  [ -f "$2" ] || curl -sSL --max-time 600 -o "$2" "$1"
}

build_zlib() {
  echo "[zlib] 1.3.1"
  fetch "https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz" "$WORK/zlib.tar.gz"
  tar -xzf "$WORK/zlib.tar.gz" -C "$WORK"
  local d="$WORK/zlib-1.3.1" o="$WORK/zo"
  mkdir -p "$o"
  local objs=()
  for f in adler32 compress crc32 deflate infback inffast inflate inftrees trees uncompr zutil; do
    "$ZIG_BIN" cc -target aarch64-windows-gnu -O2 -c -o "$o/$f.o" "$d/$f.c"
    objs+=("$o/$f.o")
  done
  "$ZIG_BIN" ar rcs "$OUT/lib/libz.a" "${objs[@]}"
  cp "$d/zlib.h" "$d/zconf.h" "$OUT/include/"
}

build_sqlite3() {
  echo "[sqlite3] amalgamation 3.46.1"
  fetch "https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip" "$WORK/sqlite.zip"
  ( cd "$WORK" && unzip -q -o sqlite.zip )
  local d="$WORK/sqlite-amalgamation-3460100"
  "$ZIG_BIN" cc -target aarch64-windows-gnu -O2 -c -o "$WORK/sqlite3.o" "$d/sqlite3.c"
  "$ZIG_BIN" ar rcs "$OUT/lib/libsqlite3.a" "$WORK/sqlite3.o"
  cp "$d/sqlite3.h" "$d/sqlite3ext.h" "$OUT/include/"
}

for d in $DEPS; do
  case "$d" in
    zlib)    build_zlib ;;
    sqlite3) build_sqlite3 ;;
    *)       echo "[$d] non ancora supportato: aggiungi il caso in questo script" >&2 ;;
  esac
done
echo "dipendenze in $OUT"
ls -1 "$OUT/lib" || true
