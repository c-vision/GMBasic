#!/usr/bin/env bash
# Costruisce il pacchetto duckdb: shim dylib SELF-CONTAINED (libduckdb staticamente
# linkata dentro) + header. Prerequisito: DuckDB installato (brew --prefix duckdb).
# Output in lib/<platform>/libgmbduckdb.dylib (un solo file, nessuna lib esterna).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
LIB="$HERE/../../../gmbduckdb"
DUCKDB=$(brew --prefix duckdb)
[ -n "$DUCKDB" ] || { echo "DuckDB non trovato (brew install duckdb)"; exit 1; }
OUT="$HERE/lib/osx-arm64"
mkdir -p "$OUT" "$HERE/include"
rm -f "$OUT/libgmbduckdb.dylib" "$OUT/libduckdb.dylib"
cc -dynamiclib -O2 -I"$DUCKDB/include" -I"$LIB/include" -DGMB_DUCKDB "$LIB/src/gmbduckdb.c" \
   -o "$OUT/libgmbduckdb.dylib" \
   "$DUCKDB/lib/libduckdb_static.a" "$DUCKDB/lib/libdummy_static_extension_loader.a" \
   -lc++ -lpthread -ldl -lm
cp "$LIB/include/gmbduckdb.h" "$HERE/include/gmbduckdb.h"
echo "ok: $OUT/libgmbduckdb.dylib (self-contained)"
