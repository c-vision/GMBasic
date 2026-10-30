#!/usr/bin/env bash
# ffi_build_linux_inner.sh — compila le librerie FFI dentro un container Linux (parte interna di
# `ffi_build_cross.sh`). Non si esegue a mano.
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
: "${LIBS:?}"; : "${SRC:?}"; : "${OUT:?}"

apt-get update -qq
apt-get install -y -qq --no-install-recommends gcc libc6-dev >/dev/null

built=0
for lib in $LIBS; do
  d="$SRC/$lib"
  srcs=$(find "$d/src" -name '*.c' | sort)
  [ -n "$srcs" ] || continue
  # shellcheck disable=SC2086
  gcc -std=gnu99 -D_DEFAULT_SOURCE -O2 -fPIC -shared -I"$d/include" -I"$d/src" -I"$SRC/../include" \
      $srcs -o "$OUT/libgmb$lib.so" 2>/dev/null && built=$((built+1)) || echo "  $lib: build FALLITA"
done
echo "ffi linux: $built librerie in $OUT"
