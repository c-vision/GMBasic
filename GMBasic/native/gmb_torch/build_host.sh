#!/bin/sh
# ============================================================================
# build_host.sh — compila l'host ML (libgmb_torch) e lo copia in Resources/libs/<rid>/
# ============================================================================
# Uso:
#   TORCH_PATH=<venv o distro libtorch> ./build_host.sh
# (TORCH_PATH può anche venire dall'env o da CMake -DTORCH_PATH)
# Output: ../Resources/libs/<rid>/libgmb_torch.{dylib,so,dll}
# ============================================================================
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"

if [ -z "$TORCH_PATH" ]; then
    for cand in \
        "$HOME/ai/env312/lib/python3.12/site-packages/torch" \
        "$HOME/libtorch" /usr/local/libtorch; do
        [ -d "$cand/include" ] && [ -d "$cand/lib" ] && { TORCH_PATH="$cand"; break; }
    done
fi
if [ -z "$TORCH_PATH" ]; then
    echo "TORCH_PATH non trovato (venv/libtorch). Esportalo esplicitamente." >&2
    exit 1
fi
echo "TORCH_PATH=$TORCH_PATH"

case "$(uname -s)-$(uname -m)" in
    Darwin-arm64) RID=osx-arm64; EXT=dylib;;
    Darwin-x86_64) RID=osx-x64; EXT=dylib;;
    Linux-aarch64) RID=linux-arm64; EXT=so;;
    Linux-x86_64) RID=linux-x64; EXT=so;;
    *) echo "RID non riconosciuto" >&2; exit 1;;
esac
OUT="$ROOT/Resources/libs/$RID/libgmb_torch.$EXT"
mkdir -p "$(dirname "$OUT")"

CXX="${CXX:-clang++}"
$CXX -std=c++17 -shared -fPIC -O2 \
    -I "$HERE/include" \
    -I "$TORCH_PATH/include" -I "$TORCH_PATH/include/torch/csrc/api/include" \
    "$HERE/src/gmb_host_ml.cpp" \
    -L "$TORCH_PATH/lib" -ltorch -ltorch_cpu -lc10 -lomp \
    -Wl,-rpath,@loader_path -Wl,-install_name,@rpath/libgmb_torch.$EXT \
    -o "$OUT"

# libomp del venv ha install_name assoluto inesistente → reference @rpath (dev).
# Le torch lib usano @rpath/lib*.dylib: con rpath @loader_path vengono trovate
# nella STESSA cartella del host (Resources/libs/<rid>/ o cache ~/.gmbasic/ml/<rid>/).
if [ "$EXT" = "dylib" ]; then
    install_name_tool -change "/opt/llvm-openmp/lib/libomp.dylib" "@rpath/libomp.dylib" "$OUT" 2>/dev/null || true
fi
echo "OK → $OUT"
