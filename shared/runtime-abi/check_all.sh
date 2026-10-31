#!/bin/sh
# check_all.sh — gate ABI: verifica che le dylib C consumate da Sharp espongano davvero i
# simboli elencati nei file `*.symbols` (il contratto C → C#, vedi README e LICENSING.md).
#
# DOVE CERCA LE DYLIB (stesso ordine del loader, `gmb_boot.c::gmb_lib_path`):
#   1. <monorepo>/GMBasic/build/libs/<rid>/      ← build locale del checkout (sviluppo)
#   2. ~/.gmbasic/libs/<rid>/                    ← cache utente (bundle pubblicati)
#   3. ~/.gmbasic/libs/ml/<rid>/                 ← componenti ML (torch loader)
#   4. <monorepo>/GMBasic/native/gmb_db_c/build/ ← gmb_db (prodotta dal suo progetto)
#
# PRIMA questo script cercava in `GMBasic/Resources/libs` e `GMBasic/libs-runtime`, cartelle
# CANCELLATE: 12 lib su 13 risultavano SKIP e lo script stampava comunque "ABI: OK" — un gate
# che non controllava nulla (e il drift dei simboli è passato inosservato). Ora una lib attesa
# e ASSENTE è un ERRORE, salvo `--allow-missing` (per chi non ha ancora compilato tutto).
#
# USO
#   shared/runtime-abi/check_all.sh [--allow-missing|-h]
# Exit: 0 = tutti i simboli presenti; 1 = manca un simbolo o una dylib attesa.
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
G="$ROOT/GMBasic"

ALLOW_MISSING=0
for a in "$@"; do
  case "$a" in
    --allow-missing) ALLOW_MISSING=1 ;;
    -h|--help) sed -n '2,24p' "$0"; exit 0 ;;
    *) echo "opzione sconosciuta: $a" >&2; exit 2 ;;
  esac
done

# ── RID (stessa mappatura di native/cmake/GmbRid.cmake, LibsResolver.Rid, gmb_libs_rid) ──────
case "$(uname -s)" in
  Darwin) case "$(uname -m)" in arm64) RID=osx-arm64 ;; *) RID=osx-x64 ;; esac ;;
  Linux)  case "$(uname -m)" in aarch64|arm64) RID=linux-arm64 ;; *) RID=linux-x64 ;; esac ;;
  *) echo "RID non riconosciuto ($(uname -s)/$(uname -m))" >&2; exit 2 ;;
esac

BUILD="$G/build/libs/$RID"
CACHE="$HOME/.gmbasic/libs/$RID"
ML="$HOME/.gmbasic/libs/ml/$RID"
DBBUILD="$G/native/gmb_db_c/build"

BIN="$HERE/build/check_abi"
if [ ! -x "$BIN" ]; then
  cmake -S "$HERE" -B "$HERE/build" >/dev/null
  cmake --build "$HERE/build" >/dev/null
fi

# Le dylib hanno dipendenze @rpath: serve il loader path, NON è configurazione di runtime.
DYLD_LIBRARY_PATH="$BUILD:$CACHE:$ML"; export DYLD_LIBRARY_PATH
LD_LIBRARY_PATH="$BUILD:$CACHE:$ML"; export LD_LIBRARY_PATH

find_lib() { # <nome-file> → path, 0 se trovata
  for d in "$BUILD" "$CACHE" "$ML" "$DBBUILD"; do
    if [ -f "$d/$1" ]; then echo "$d/$1"; return 0; fi
  done
  return 1
}

fail=0; missing=0; checked=0

check() { # <nome-file> <symbols> <etichetta>
  p="$(find_lib "$1")" || {
    missing=$((missing + 1))
    if [ "$ALLOW_MISSING" = "1" ]; then
      echo "SKIP $3 ($1 non compilata: né build né cache)"
    else
      echo "MANCA $3 ($1 non compilata: né build né cache)"
      fail=1
    fi
    return 0
  }
  checked=$((checked + 1))
  "$BIN" "$p" "$2" || fail=1
}

check libgmb_db.dylib              "$HERE/gmb_db.symbols"              gmb_db
check libgmb_gfx.dylib             "$HERE/gmb_gfx.symbols"             gmb_gfx
check libgmb_ai_llm.dylib          "$HERE/gmb_ai_llm.symbols"          gmb_ai_llm
check libgmb_ai_modelio.dylib      "$HERE/gmb_ai_modelio.symbols"      gmb_ai_modelio
check libgmb_ai_mlx.dylib          "$HERE/gmb_ai_mlx.symbols"          gmb_ai_mlx
check libgmb_ai_finetune.dylib     "$HERE/gmb_ai_finetune.symbols"     gmb_ai_finetune
check libllm_ai_llamacpp.dylib     "$HERE/llm_ai_llamacpp.symbols"     llm_ai_llamacpp
check libllmtoken.dylib            "$HERE/llmtoken.symbols"            llmtoken
check libgmb_torch_loader.dylib    "$HERE/gmb_torch_loader.symbols"    gmb_torch_loader
check libgmb_compress.dylib        "$HERE/gmb_compress.symbols"        gmb_compress
check libgmb_grep.dylib            "$HERE/gmb_grep.symbols"            gmb_grep
check libgmb_pkg.dylib             "$HERE/gmb_pkg.symbols"             gmb_pkg
check libllmengine_moe_stream.dylib "$HERE/llmengine_moe_stream.symbols" llmengine_moe_stream
# x64-only su host arm (gate su runner x64): gmb_tray, llm_vision_apple

if [ "$fail" = "0" ]; then
  echo "ABI $(cat "$HERE/ABI_VERSION"): OK ($checked dylib verificate, $missing assenti)"
else
  echo "ABI $(cat "$HERE/ABI_VERSION"): FAIL ($checked verificate, $missing assenti)"
fi
exit "$fail"
