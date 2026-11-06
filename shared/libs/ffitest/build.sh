#!/usr/bin/env bash
# build.sh — compila libffitest (test FFI >8 argomenti) e la installa dove il loader
# del runtime la trova: shared/libs/ (cwd del repo) e ~/.gmbasic/libs/<rid>/.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
MONO="$(cd "$HERE/../../.." && pwd)"
RID="$(uname -s | tr '[:upper:]' '[:lower:]')-$(uname -m)"
case "$(uname -m)" in arm64) RID="osx-arm64";; x86_64) RID="osx-x64";; esac
OUT="$HERE/libffitest.dylib"
cc -shared -O2 -o "$OUT" "$HERE/ffitest.c"
mkdir -p "$MONO/shared/libs" "$HOME/.gmbasic/libs/$RID"
cp -f "$OUT" "$MONO/shared/libs/libffitest.dylib"
cp -f "$OUT" "$HOME/.gmbasic/libs/$RID/libffitest.dylib"
echo "[ffitest] $OUT (+ shared/libs, ~/.gmbasic/libs/$RID)"
