#!/usr/bin/env bash
# ffi_build_cross.sh — compila le NOSTRE librerie FFI per un RID che NON è quello della macchina.
#
# Le 7 librerie (amqp, grpc, kafka, mqtt, nats, protobuf, media) vivono in `shared/libs/<nome>/`
# e non hanno dipendenze esterne (solo libc/POSIX socket). Su Windows serve lo strato
# `GMBasic/native/win32shim/include` (POSIX→Win32), su macOS il SDK, su Linux un container con gcc.
# `media` usa `arm_neon.h`: si compila solo per ARM.
#
# USO
#   tools/sh/ffi_build_cross.sh <rid> [--out DIR]
#     rid: osx-x64 | win-x64 | win-arm64 | linux-x64 | linux-arm64
#
# Poi:
#   tools/sh/libs_publish_files_gh.sh ffi <rid> --source <out> --tag ffi-v2 --yes
set -uo pipefail

RID="${1:-}"; shift || true
OUT="${HOME}/ffi-stage/${RID}"
while [ $# -gt 0 ]; do
  case "$1" in --out) OUT="${2:-}"; shift 2 ;; *) echo "opzione sconosciuta: $1" >&2; exit 2 ;; esac
done
[ -n "$RID" ] || { sed -n '2,18p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }

MONO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LIBS="$MONO/shared/libs"
INC="$MONO/shared/include"
WINC="$MONO/GMBasic/native/win32shim/include"
ZIG="${ZIG:-$HOME/ai/tools/zig-aarch64-macos-0.17.0/zig}"
SDK="$(xcrun --sdk macosx --show-sdk-path 2>/dev/null || true)"

# `media` richiede NEON: solo ARM.
ALL="amqp grpc kafka mqtt nats protobuf"
case "$RID" in
  *arm64*) ALL="$ALL media" ;;
esac

mkdir -p "$OUT"
built=0
for lib in $ALL; do
  d="$LIBS/$lib"
  srcs=$(find "$d/src" -name '*.c' | sort)
  [ -n "$srcs" ] || { echo "  $lib: sorgenti assenti"; continue; }
  case "$RID" in
    osx-x64)
      # shellcheck disable=SC2086
      clang -std=c99 -O2 -fPIC -dynamiclib -target x86_64-apple-darwin -isysroot "$SDK" \
        -install_name "@rpath/libgmb$lib.dylib" \
        -I"$d/include" -I"$d/src" -I"$INC" $srcs -o "$OUT/libgmb$lib.dylib" 2>/dev/null \
        && built=$((built+1)) || echo "  $lib: build FALLITA"
      ;;
    *-windows-*|win-*)
      case "$RID" in win-arm64) TRIPLE=aarch64-windows-gnu ;; *) TRIPLE=x86_64-windows-gnu ;; esac
      # shellcheck disable=SC2086
      "$ZIG" cc -std=c99 -O2 -shared -target "$TRIPLE" -DGMB_WIN32_SOCKET_IO \
        -I"$d/include" -I"$d/src" -I"$INC" -I"$WINC" -include "$WINC/gmb_win32.h" \
        $srcs -o "$OUT/libgmb$lib.dll" -lws2_32 2>/dev/null \
        && built=$((built+1)) || echo "  $lib: build FALLITA"
      ;;
    linux-*)
      # Tutte le librerie insieme, in un container della stessa architettura del RID.
      case "$RID" in linux-arm64) PLAT=linux/arm64 ;; *) PLAT=linux/amd64 ;; esac
      docker run --rm --platform "$PLAT" -v "$MONO":/mono:ro -v "$OUT":/out \
        -e LIBS="$ALL" -e SRC=/mono/shared/libs -e OUT=/out \
        ubuntu:24.04 bash /mono/tools/sh/ffi_build_linux_inner.sh || { echo "  container FFI fallito" >&2; exit 1; }
      built=$(ls -1 "$OUT"/*.so 2>/dev/null | wc -l | tr -d ' ')
      break
      ;;
    *) echo "RID sconosciuto: $RID" >&2; exit 2 ;;
  esac
done

echo "ffi $RID → $OUT: $built librerie"
ls -1 "$OUT" 2>/dev/null | sed 's/^/  /'
