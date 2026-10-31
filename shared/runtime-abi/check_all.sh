#!/bin/sh
# check_all.sh — gate ABI locale: verifica le dylib C consumate da Sharp.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
G="$ROOT/GMBasic"
BIN="$HERE/build/check_abi"
if [ ! -x "$BIN" ]; then
  cmake -S "$HERE" -B "$HERE/build" >/dev/null
  cmake --build "$HERE/build" >/dev/null
fi
# Dipendenze delle dylib (rpath @rpath): loader path per il checker, non configurazione runtime.
DYLD_LIBRARY_PATH="$HOME/.gmbasic/libs/osx-arm64:$HOME/.gmbasic/libs/ml/osx-arm64"
export DYLD_LIBRARY_PATH
fail=0
check() { # check <dylib> <symbols> <nome>
  if [ ! -f "$1" ]; then echo "SKIP $3 (assente: $1)"; return 0; fi
  "$BIN" "$1" "$2" || fail=1
}
R="$G/Resources/libs/osx-arm64"; L="$G/libs-runtime/osx-arm64"
check "$G/native/gmb_db_c/build/libgmb_db.dylib" "$HERE/gmb_db.symbols" "gmb_db"
check "$L/libgmb_gfx.dylib" "$HERE/gmb_gfx.symbols" "gmb_gfx"
check "$L/libgmb_ai_llm.dylib" "$HERE/gmb_ai_llm.symbols" "gmb_ai_llm"
check "$R/libgmb_ai_modelio.dylib" "$HERE/gmb_ai_modelio.symbols" "gmb_ai_modelio"
check "$R/libgmb_ai_mlx.dylib" "$HERE/gmb_ai_mlx.symbols" "gmb_ai_mlx"
check "$R/libgmb_ai_finetune.dylib" "$HERE/gmb_ai_finetune.symbols" "gmb_ai_finetune"
check "$R/libllm_ai_llamacpp.dylib" "$HERE/llm_ai_llamacpp.symbols" "llm_ai_llamacpp"
check "$L/libllmtoken.dylib" "$HERE/llmtoken.symbols" "llmtoken"
check "$R/libgmb_torch_loader.dylib" "$HERE/gmb_torch_loader.symbols" "gmb_torch_loader"
check "$L/libgmb_compress.dylib" "$HERE/gmb_compress.symbols" "gmb_compress"
check "$L/libgmb_grep.dylib" "$HERE/gmb_grep.symbols" "gmb_grep"
check "$R/libgmb_pkg.dylib" "$HERE/gmb_pkg.symbols" "gmb_pkg"
check "$R/libllmengine_moe_stream.dylib" "$HERE/llmengine_moe_stream.symbols" "llmengine_moe_stream"
# x64-only su host arm: verificare su runner x64
[ "$fail" = "0" ] && echo "ABI $(cat "$HERE/ABI_VERSION"): OK" || echo "ABI: FAIL"
exit "$fail"
