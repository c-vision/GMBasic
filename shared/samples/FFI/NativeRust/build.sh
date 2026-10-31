#!/usr/bin/env sh
# ============================================================================
# build.sh — compila math_rust.rs in libmath_rust.dylib (macOS) / .so (Linux)
# ============================================================================
set -e
cd "$(dirname "$0")"

case "$(uname -s)" in
  Darwin) OUT=libmath_rust.dylib ;;
  *)      OUT=libmath_rust.so ;;
esac

rustc --crate-type cdylib math_rust.rs -o "$OUT"
echo "OK -> $OUT"
