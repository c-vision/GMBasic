#!/usr/bin/env sh
# ============================================================================
# build.sh — compila math_go.go in libmath_go.dylib (macOS) / .so (Linux)
# ============================================================================
set -e
cd "$(dirname "$0")"

case "$(uname -s)" in
  Darwin) OUT=libmath_go.dylib ;;
  *)      OUT=libmath_go.so ;;
esac

go build -buildmode=c-shared -o "$OUT" math_go.go
echo "OK -> $OUT"
