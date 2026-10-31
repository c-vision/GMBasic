#!/bin/sh
# ============================================================================
# build_loader.sh — compila il LOADER ML standalone (libgmb_torch_loader)
# ============================================================================
# Il loader è la dylib C caricata via P/Invoke dal C# (MlNativeEngine, nome
# "gmb_torch_loader"): risolve a runtime le gmb_ml_* dalla lib host
# (libgmb_torch), MA non linka il runtime C (gmb_runtime_c).
#
# gmb_torch.c chiama gmb_paths_ml_dir (definita nel runtime C, assente nel
# processo C#): qui viene fornito uno stub fedele che replica la risoluzione
# essenziale (GMB_TORCH_PATH, altrimenti la cache componente `~/.gmbasic/libs/ml/<rid>`).
#
# Output: ~/.gmbasic/libs/ml/<rid>/libgmb_torch_loader.{dylib,so}  (cache componente)
#
# Uso:  ./build_loader.sh
# ============================================================================
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"

case "$(uname -s)-$(uname -m)" in
    Darwin-arm64)  RID=osx-arm64;  EXT=dylib;;
    Darwin-x86_64) RID=osx-x64;    EXT=dylib;;
    Linux-aarch64) RID=linux-arm64; EXT=so;;
    Linux-x86_64)  RID=linux-x64;  EXT=so;;
    *) echo "RID non riconosciuto" >&2; exit 1;;
esac

OUT="${HOME}/.gmbasic/libs/ml/$RID/libgmb_torch_loader.$EXT"
mkdir -p "$(dirname "$OUT")"

STUB="$(mktemp "${TMPDIR:-/tmp}/gmb_loader_stub.XXXXXX.c")"
cat > "$STUB" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
void gmb_paths_ml_dir(const char* rid, char* out, size_t cap)
{
    const char* env = getenv("GMB_TORCH_PATH");
    if (env && *env) { snprintf(out, cap, "%s", env); return; }
    /* componente TORCH: cache utente (`~/.gmbasic/libs/ml/<rid>`) */
    snprintf(out, cap, "%s/.gmbasic/libs/ml/%s", getenv("HOME") ? getenv("HOME") : ".", rid ? rid : "");
}

/* gmb_opt_env: ponte env->registro del runtime C (assente nel processo C#/loader). Qui NON c'e'
 * registro -> si limita al fallback d'ambiente (getenv). SENZA questo stub il loader resta con
 * `_gmb_opt_env` indefinito e dlopen(RTLD_NOW) fallisce ("symbol not found in flat namespace"). */
const char* gmb_opt_env(const char* name) { return name ? getenv(name) : NULL; }
EOF

CC="${CC:-cc}"
if [ "$EXT" = "dylib" ]; then
    "$CC" -std=c11 -shared -fPIC -O2 \
        -I "$HERE/include" -I "$ROOT/native/gmb_runtime_c/include" \
        "$HERE/src/gmb_torch.c" "$STUB" \
        -Wl,-undefined,dynamic_lookup \
        -Wl,-install_name,@rpath/libgmb_torch_loader.dylib \
        -o "$OUT"
else
    "$CC" -std=c11 -shared -fPIC -O2 \
        -I "$HERE/include" -I "$ROOT/native/gmb_runtime_c/include" \
        "$HERE/src/gmb_torch.c" "$STUB" \
        -o "$OUT"
fi
rm -f "$STUB"
echo "OK → $OUT"
