#!/usr/bin/env bash
# torch_build.sh — compila i DUE binari del componente TORCH e li prepara per la pubblicazione.
#
#   libgmb_torch         host C++ (src/gmb_host_ml.cpp) che implementa gmb_ml.h sopra libtorch
#   libgmb_torch_loader  loader C standalone (src/gmb_torch.c + stub), P/Invoke dal C# (MlNativeEngine)
#
# Perché serve compilarli: NON sono nell'archivio pytorch (quello porta solo libtorch/libc10/libomp),
# quindi vanno costruiti e pubblicati a parte — release `torch-v1` di c-vision/GMBasicModules,
# referenziata da `shared/settings/ml-manifest.json` con source="url".
#
# USO
#   tools/sh/torch_build.sh osx-arm64                 # host macOS: usa il torch della venv (~/ai/env312)
#   tools/sh/torch_build.sh linux-x64                 # via container ubuntu:24.04 (g++ + libtorch ufficiale)
#   TORCH_PATH=/path/to/torch tools/sh/torch_build.sh osx-arm64
#
# Output: ~/torch-build/<rid>/{libgmb_torch,libgmb_torch_loader}-<rid>.<ext>
# Poi:    gh release upload torch-v1 <file>... --repo c-vision/GMBasicModules --clobber
#         e AGGIORNA sha256/size in shared/settings/ml-manifest.json (il binario Mach-O
#         porta un LC_UUID: due build non sono mai byte-identiche).
#
# RID non compilabili (verificato 2026-10-30 su download.pytorch.org 2.13.0):
#   osx-x64, linux-arm64 → PyTorch non pubblica libtorch per quelle architetture
#   win-x64, win-arm64   → libtorch è MSVC: servono MSVC/Windows (la cross-build usa zig/mingw)
set -uo pipefail

RID="${1:-}"
[ -n "$RID" ] || { sed -n '2,25p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }

MONO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC="$MONO/GMBasic/native/gmb_torch"
RUNTIME_INC="$MONO/GMBasic/native/gmb_runtime_c/include"
OUT="${HOME}/torch-build/$RID"
mkdir -p "$OUT"

VERSION="2.13.0"

# ── stub del loader (assente nel processo C#: niente runtime C) ────────────────────────────────
make_loader_stub() {
  local f="$1"
  cat > "$f" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
void gmb_paths_ml_dir(const char* rid, char* out, size_t cap)
{
    const char* env = getenv("GMB_TORCH_PATH");
    if (env && *env) { snprintf(out, cap, "%s", env); return; }
    snprintf(out, cap, "%s/.gmbasic/libs/ml/%s", getenv("HOME") ? getenv("HOME") : ".", rid ? rid : "");
}
const char* gmb_opt_env(const char* name) { return name ? getenv(name) : NULL; }
EOF
}

build_native() {   # $1=RID  $2=dylib|so  $3=torch path
  local rid="$1" ext="$2" tp="$3"
  local cxx="${CXX:-clang++}" cc="${CC:-cc}" stub
  echo "[$rid] host ML (libgmb_torch.$ext) contro $tp"
  "$cxx" -std=c++17 -shared -fPIC -O2 -I "$SRC/include" \
      -I "$tp/include" -I "$tp/include/torch/csrc/api/include" \
      "$SRC/src/gmb_host_ml.cpp" -L "$tp/lib" -ltorch -ltorch_cpu -lc10 -lomp \
      -Wl,-rpath,@loader_path -Wl,-install_name,@rpath/libgmb_torch.$ext \
      -o "$OUT/libgmb_torch-$rid.$ext" || return 1
  if [ "$ext" = dylib ]; then
    # libomp della venv ha install_name assoluto: reference @rpath
    install_name_tool -change "/opt/llvm-openmp/lib/libomp.dylib" "@rpath/libomp.dylib" \
        "$OUT/libgmb_torch-$rid.$ext" 2>/dev/null || true
  fi
  stub=$(mktemp "${TMPDIR:-/tmp}/gmb_loader_stub.XXXXXX.c"); make_loader_stub "$stub"
  echo "[$rid] loader (libgmb_torch_loader.$ext)"
  "$cc" -std=c11 -shared -fPIC -O2 -I "$SRC/include" -I "$RUNTIME_INC" \
      "$SRC/src/gmb_torch.c" "$stub" \
      ${ext:+$([ "$ext" = dylib ] && printf '%s' '-Wl,-undefined,dynamic_lookup -Wl,-install_name,@rpath/libgmb_torch_loader.dylib')} \
      -o "$OUT/libgmb_torch_loader-$rid.$ext" || { rm -f "$stub"; return 1; }
  rm -f "$stub"
}

case "$RID" in
  osx-arm64|osx-x64)
    TP="${TORCH_PATH:-$HOME/ai/env312/lib/python3.12/site-packages/torch}"
    [ -d "$TP/include" ] || { echo "TORCH_PATH non valido: $TP" >&2; exit 1; }
    build_native "$RID" dylib "$TP"
    ;;
  linux-x64|linux-arm64)
    PLAT=$([ "$RID" = linux-arm64 ] && echo linux/arm64 || echo linux/amd64)
    ZIP="libtorch-shared-with-deps-$VERSION+cpu.zip"
    W="${HOME}/torch-build/.work-$RID"; mkdir -p "$W"
    [ -f "$W/$ZIP" ] || curl -sL --max-time 1800 \
        -o "$W/$ZIP" "https://download.pytorch.org/libtorch/cpu/$ZIP"
    [ -d "$W/dist/libtorch" ] || unzip -q -o "$W/$ZIP" -d "$W/dist"
    cat > "$W/inner.sh" <<'INNER'
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq; apt-get install -y -qq --no-install-recommends g++ unzip >/dev/null
OUT=/out; SRC=/src; RT=/rt; TP=/w/dist/libtorch
g++ -std=c++17 -shared -fPIC -O2 -I "$SRC/include" \
    -I "$TP/include" -I "$TP/include/torch/csrc/api/include" \
    "$SRC/src/gmb_host_ml.cpp" -L "$TP/lib" -ltorch -ltorch_cpu -lc10 \
    -Wl,-rpath,'$ORIGIN' -o "$OUT/libgmb_torch-linux.so"
INNER
    docker run --rm --platform "$PLAT" -v "$SRC":/src:ro -v "$RUNTIME_INC":/rt:ro \
        -v "$W":/w -v "$OUT":/out ubuntu:24.04 bash /w/inner.sh || exit 1
    mv "$OUT/libgmb_torch-linux.so" "$OUT/libgmb_torch-$RID.so"
    stub=$(mktemp "${TMPDIR:-/tmp}/gmb_loader_stub.XXXXXX.c"); make_loader_stub "$stub"
    cc -std=c11 -shared -fPIC -O2 -I "$SRC/include" -I "$RUNTIME_INC" \
        "$SRC/src/gmb_torch.c" "$stub" -o "$OUT/libgmb_torch_loader-$RID.so"
    rm -f "$stub"
    ;;
  win-x64|win-arm64)
    echo "RID $RID: libtorch è MSVC — serve MSVC su Windows (la cross-build zig/mingw non è ABI-compatibile)." >&2
    exit 4
    ;;
  *) echo "RID sconosciuto: $RID" >&2; exit 2 ;;
esac

echo "OK → $OUT"
ls -l "$OUT" | sed 's/^/  /'
