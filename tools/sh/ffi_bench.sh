#!/usr/bin/env bash
# ffi_bench.sh — costo di una chiamata FFI sulle 4 gambe (IL, PCodeVM, VM/C, C99Native).
# Il programma misura se stesso con ft_now_ms (libffitest): ns per chiamata FFI
# (double -> double) e ns per una addizione locale nello stesso loop.
#
# Uso: tools/sh/ffi_bench.sh [FILE.bas]
# Default: shared/bench/ffi_roundtrip.bas
set -u
export LC_ALL=C
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
FILE="${1:-$ROOT/shared/bench/ffi_roundtrip.bas}"
[ -f "$FILE" ] || { echo "file non trovato: $FILE" >&2; exit 2; }

run() { # nome comando
  local name="$1"; shift
  local out
  out="$("$@" 2>&1)" || true
  local ffi base ffi12
  ffi="$(echo "$out" | sed -n 's/^ffi_ns= *//p' | tail -1)"
  base="$(echo "$out" | sed -n 's/^base_ns= *//p' | tail -1)"
  ffi12="$(echo "$out" | sed -n 's/^ffi12_ns= *//p' | tail -1)"
  if [ -n "$ffi" ]; then
    printf '%-8s ffi=%6s ns/chiamata   ffi12=%6s ns   base=%6s ns/loop\n' \
      "$name" "$ffi" "$ffi12" "$base"
  else
    printf '%-8s FAIL\n' "$name"
    echo "$out" | tail -3 | sed 's/^/         /'
  fi
}

# `dotnet run` (non il binario precompilato): rebuilda il runtime C# dopo una modifica,
# altrimenti si misura una DLL vecchia (anti-stale, come fa parity4).
run IL    dotnet run --project "$ROOT/tools/runners/test_ilrun" -- --engine=il "$FILE"
run PCode dotnet run --project "$ROOT/tools/runners/test_ilrun" -- --engine=pcode "$FILE"
run VM/C  "$ROOT/tools/runners/test_vmrun/bin/Debug/net10.0/test_vmrun" "$FILE" 300
run C99   bash "$ROOT/tools/sh/gmb_c99native_run.sh" "$FILE" ffi_bench
