#!/usr/bin/env bash
# ── MUTEX di build: i comandi di build (cc/clang/cmake/make/dotnet) prendono un lock ──
# esclusivo su mutex.txt SOLO per la durata del SINGOLO comando, poi lo rilasciano subito
# (lo script puo proseguire a lungo senza tenere il lock). Scadenza 15 min con heartbeat:
# un processo in hang non blocca le altre AI. Vedi tools/gmb_with_lock.sh e Knowledge/
# Build-Mutex-Lock-AI.md.
if [ -z "${GMB_BUILDBIN_ON:-}" ]; then
  export GMB_BUILDBIN_ON=1
  GMB_BUILDBIN="$(cd "$(dirname "$0")" && pwd)/buildbin"
  [ -d "$GMB_BUILDBIN" ] && export PATH="$GMB_BUILDBIN:$PATH"
fi

# tools/sh/gmb_c99native_run.sh — compila+linka+esegue l'output del driver C99Native.
# Uso: gmb_c99native_run.sh <sample.bas> [suffisso] [cflags]
#   CWD = root del repo. Emette in <dir>/work/<base>_<suff>.c e lo esegue.
# Serve per la verifica END-TO-END del C99Native (EVAL/&, locali S_eval, ...): il driver
# produce C; qui lo si linka col lib-set completo di gmb_vm_host (runtime+db+agent+doc +
# SDL/OpenSSL/curl dylib di Homebrew) e lo si esegue. Prerequisiti: build nativo esistente
# (native/gmb_vm_c/build), Homebrew libs. Esempio: canarini Clipper* LocalVar 7/7.
#
# I FLAG DI COMPILAZIONE non sono fissi: si leggono dalla sezione "compiler" di
# Resources/system.settings.json (default GLOBALI del compilatore), con override
# per-applicazione da <dir>/app.settings.json — stessa semantica documentata in
# common/CommonLib/SettingsRecords.cs (CompilerSettings.Resolve), che resta la fonte
# canonica; questo script la rispecchia per non avere bisogno del CLR.
# Il 3° argomento (cflags) SOSTITUISCE i cflags letti dai settings.
# NESSUNA variabile d'ambiente: i parametri passano da settings o da argomento.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../../GMBasic" && pwd)
TOOLS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"   # tools/ (script ausiliari)
# Cache ON-DEMAND delle librerie native (CommonLib.LibsResolver / `gmb libs ensure`): il link usa
# le copie della CACHE, che hanno install-name `@rpath/...`, NON i dist di build (possono essere
# stale e avere `@loader_path`: caso reale `libllmtoken` @loader_path → l'eseguibile non partiva
# con "Library not loaded: @loader_path/libllmtoken.dylib").
case "$(uname -s)-$(uname -m)" in
  Darwin-arm64)  RID=osx-arm64;;
  Darwin-x86_64) RID=osx-x64;;
  Linux-x86_64)  RID=linux-x64;;
  Linux-aarch64) RID=linux-arm64;;
  *)             RID=osx-arm64;;
esac
LIBS_CACHE="$HOME/.gmbasic/libs/$RID"

# ── Download on-demand (asset per-file) ─────────────────────────────────────────────────────
# `libs.txt` = indice ridotto per il downloader C: nome<TAB>sha256<TAB>size<TAB>url.
# Lo emette dal manifest (solo i file che hanno un `url`, cioè pubblicati per-file).
# Serve a DUE cose: (a) il COMPILATORE C che scarica le librerie mancanti prima del link,
# (b) l'eseguibile compilato, che porta accanto `<exe>.libs.txt` e scarica da solo ciò che gli
# serve quando `dlopen` non trova la lib (gmb_ffi_dl → gmb_libs_autoensure).
emit_libs_index() { # $1 = file di output  (tutti i file con url del RID)
  # L'indice TSV lo genera uno script Python separato: qui niente heredoc con escape
  # (la versione inline era stata scritta male e produceva un file vuoto in silenzio).
  python3 "$TOOLS_DIR/../data/libs_index_tsv.py" all "$ROOT/../shared/settings/libs-manifest.json" "$RID" "$1" 2>/dev/null || true
}

# Post-link: `<exe>.libs.txt` con le lib del manifest da cui l'eseguibile DIPENDE davvero
# (letto dal binario prodotto: otool -L / readelf -d). Così l'exe scarica solo le sue.
emit_exe_libs() { # $1 = eseguibile, $2 = output  (solo le lib del manifest da cui dipende)
  local exe="$1" out="$2" manifest="$ROOT/../shared/settings/libs-manifest.json"
  [ -f "$manifest" ] || return 0
  local names=""
  case "$(uname -s)" in
    Darwin) names=$(otool -L "$exe" 2>/dev/null | tail -n +2 | awk '{print $1}' | sed 's|.*/||') ;;
    *)      names=$(readelf -d "$exe" 2>/dev/null | sed -n 's/.*\[\(.*\)\]/\1/p') ;;
  esac
  [ -n "$names" ] || return 0
  # CHIUSURA TRANSITIVA: una lib del manifest può dipendere da un'ALTRA lib del manifest che
  # l'eseguibile non linka direttamente (caso reale: libbrotlidec → libbrotlicommon). Senza
  # questo, l'exe parte e muore su "Library not loaded: libbrotlicommon.1.dylib".
  local queue="$names" seen=" " n f dep
  while [ -n "$queue" ]; do
    local next=""
    for n in $queue; do
      case "$seen" in *" $n "*) continue;; esac
      seen="$seen$n "
      f=""
      for d in "$LIBS_CACHE" "$ROOT/build/libs/$RID"; do
        [ -f "$d/$n" ] && { f="$d/$n"; break; }
      done
      [ -n "$f" ] || continue
      for dep in $(otool -L "$f" 2>/dev/null | tail -n +2 | awk '{print $1}' | sed 's|.*/||'); do
        case "$seen" in *" $dep "*) ;; *) next="$next $dep";; esac
      done
    done
    queue="$next"
    names="$names $next"
  done
  # shellcheck disable=SC2086
  python3 "$TOOLS_DIR/../data/libs_index_tsv.py" needed "$manifest" "$RID" "$out" $names 2>/dev/null || true
}

B="${1:?uso: $0 <file.bas> [suffisso] [cflags]}"
SUFF="${2:-out}"
CF_OVERRIDE="${3:-}"
cd "$ROOT"
# Driver: si preferisce quello del BUILD del compilatore, se più recente di quello in tools/.
# La copia in tools/ diventa stantia dopo ogni modifica all'emitter/parser nativo e il sintomo è
# "driver fallito" senza dettaglio (misurato: RDD_Demo non emetteva dopo l'aggiunta degli
# statement RDD SET, mentre il driver del build emetteva correttamente).
# Driver SEMPRE fresco: il target CMake incrementale evita il caso "driver stantio" che
# faceva fallire la risoluzione dei simboli (es. SPEED.SETDURABILITY).
BUILD_DIR="$ROOT/native/gmb_compiler_c/build"
if [ -d "$BUILD_DIR" ]; then
  cmake --build "$BUILD_DIR" --target gmb_c99native_driver -j4 >/dev/null 2>&1 || true
fi
BIN="tools/gmb_c99native_driver"
BUILT="native/gmb_compiler_c/build/gmb_c99native_driver"
if [ -x "$BUILT" ] && { [ ! -x "$BIN" ] || [ "$BUILT" -nt "$BIN" ]; }; then
  BIN="$BUILT"
fi
[ -x "$BIN" ] || { echo "driver assente: $BIN (build tools/)"; exit 2; }
# Profilo C: bootstrap deterministico (i settings del compilatore sono letti dal toolchain C;
# qui niente Python). CF_OVERRIDE permette di forzare i flag.
CF="${CF_OVERRIDE:--O1 -w}"
CC_BIN="cc"

ABS=$(cd "$(dirname "$B")" && pwd)/$(basename "$B")
DIR=$(cd "$(dirname "$B")" && pwd)
mkdir -p "$DIR/work"
CASE=work/$(basename "$B" .bas)_${SUFF}


# --no-exe: il driver ora COMPILA E LINKA per default; qui si vuole il SOLO C, perché il
# link lo fa questo script col lib-set completo di gmb_vm_host (runtime+db+agent+doc+SDL).
# Lo stderr del driver non viene più nascosto: "driver fallito" senza il motivo è
# proprio il sintomo che rendeva il driver inutilmente difficile da diagnosticare.
if ! "$BIN" "$ABS" "$DIR/$CASE.c" --no-exe >/dev/null 2>"$DIR/$CASE.drivererr.txt"; then
  # In caso di fallimento il file resta (post-mortem); in caso di successo sparisce:
  # work/ è già pieno di artefatti, non aggiungiamo rumore a ogni run che va a buon fine.
  echo "driver fallito:"; sed -n '1,5p' "$DIR/$CASE.drivererr.txt"; exit 2
fi
rm -f "$DIR/$CASE.drivererr.txt"

L="$ROOT/native/gmb_vm_c/build"
# Motore/moduli AI = LLMEnv (sorgente unica); i consumer nativi non tengono piu' copie native/gmb_*.
if [ -d "$ROOT/../LLMEnv" ]; then LLMENV="$ROOT/../LLMEnv"; else LLMENV="$ROOT/../../LLMEnv"; fi
INC="-I $ROOT/native/gmb_runtime_c/include -I $ROOT/native/gmb_runtime_c/src -I $ROOT/native/gmb_chart/include -I $ROOT/native/gmb_compiler_c/include -I $LLMENV/LLMCodec/include -I $ROOT/native/gmb_agent_runtime -I $LLMENV/LLMJinja -I $ROOT/native/gmb_db_c/include -I $ROOT/native/gmb_torch/include -I $ROOT/native/gmb_doc_c/include -I $LLMENV/LLMEngine/moe/include -I $LLMENV/LLMEngine/asr/include -I $LLMENV/LLMEngine/mlx -I $LLMENV/LLMEngine/include -I $LLMENV/LLMEngine/Options -I $ROOT/gmbllm/include -I $ROOT/gmbllm/include/quant -I $ROOT/gmbllm/include/train -I $ROOT/gmbllm/include/lora -I $LLMENV/LLMEngine/plugin -I $LLMENV/LLMDesc -I $LLMENV/LLMToken/include"
# gmb_jinja (engine C dei chat-template): libgmb_agent_runtime.a lo referenzia -> va linkato.
# Jinja (LLMEnv `llmjinja`): MULTI-file (llmjinja.c + llmjinja_builtins.c) -> si linka la lib
# statica del build (non si compila il singolo .c, che lascerebbe _v_*/_p_* non definiti).
JINJA_OBJ=""
if [ -f "$L/gmb_agent_runtime_build/llmjinja_build/libllmjinja.a" ]; then
  JINJA_OBJ="$L/gmb_agent_runtime_build/llmjinja_build/libllmjinja.a"
elif [ -f "$ROOT/native/gmb_agent_runtime/build/llmjinja_build/libllmjinja.a" ]; then
  JINJA_OBJ="$ROOT/native/gmb_agent_runtime/build/llmjinja_build/libllmjinja.a"
fi
# TORCH: loader C (dlopen dell'host ML) compilato insieme al programma.
# Gli OGGETTI INTERMEDI vanno in una cartella TEMPORANEA (prima finivano nella cartella del
# programma: gmb_torch.o / gmb_torch_str.o restavano lì, sporcando la destinazione).
TMPOBJ="$(mktemp -d "${TMPDIR:-/tmp}/gmbc99obj.XXXXXX")"
trap 'rm -rf "$TMPOBJ"' EXIT
TORCH_OBJS=""
if [ -f "$ROOT/native/gmb_torch/src/gmb_torch.c" ]; then
  $CC_BIN $CF $INC -c "$ROOT/native/gmb_torch/src/gmb_torch.c" -o "$TMPOBJ/gmb_torch.o"
  TORCH_OBJS="$TMPOBJ/gmb_torch.o"
  if [ -f "$ROOT/native/gmb_torch/src/gmb_torch_str.c" ]; then
    $CC_BIN $CF $INC -c "$ROOT/native/gmb_torch/src/gmb_torch_str.c" -o "$TMPOBJ/gmb_torch_str.o"
    TORCH_OBJS="$TORCH_OBJS $TMPOBJ/gmb_torch_str.o"
  fi
fi
# `-w` sul .c GENERATO (convenzione gate C99 + test_minitests_c99native): i warning
# (es. -Wunused-value delle statement-expression) inquinerebbero lo stdout catturato
# dagli harness (falso DIFF di parity4). Gli errori restano fatali (exit!=0).
$CC_BIN $CF $INC -c "$DIR/$CASE.c" -o "$DIR/$CASE.o"
# CODEC (progetto autonomo gmb_ggml_codec): il CORE è la static lib (contenitori+registry);
# le IMPLEMENTAZIONI sono plugin DYLIB che il runtime linka (come il target CMake gmb_runtime:
# gmb_codec_ggml/mlx_affine/fp8). Senza questi, i simboli del codec (gmb_iq2_xxs_quantize,
# gmb_moe_affine_*, gmb_ggml_type_of_fmt, gmb_gguf_write) restano non definiti. Fix 2026-09-24.
CODEC="$LLMENV/LLMEngine/mlx/build-gmbasic"   # codec (dylib/a) nel build MLX di LLMEnv (post-migrazione)
# LIBRARY: il C generato non ha `int main` -> si produce una libreria condivisa (.gmblib),
# non un eseguibile; niente esecuzione (nessun entry point).
OUTBIN="$DIR/$CASE"; CCFIRST="-arch arm64"
if ! grep -q "int main" "$DIR/$CASE.c"; then OUTBIN="$DIR/$CASE.dylib"; CCFIRST="-shared -fPIC -Wl,-undefined,dynamic_lookup -arch arm64"; fi
# libllmtoken: dalla cache (install-name @rpath). Fallback al BUILD di LLMEnv, mai al dist di
# staging (variante @loader_path che non si risolve accanto all'eseguibile).
LLMTOKEN_DIR="$LIBS_CACHE"
[ -f "$LLMTOKEN_DIR/libllmtoken.dylib" ] || LLMTOKEN_DIR="$LLMENV/build/LLMToken"
# ── Quale runtime? ────────────────────────────────────────────────────────────────────────────
# TESTUALE → gmb_runtime_min (core + stub UI no-op): l'eseguibile NON dipende da SDL/Vulkan.
# FORM/READ/grafica → runtime COMPLETO (UI vera).
MIN_LIB="$ROOT/native/gmb_runtime_c/build/libgmb_runtime_min.a"
RUNTIME_LIB="$MIN_LIB"; MIN_LINK=1
if [ ! -f "$RUNTIME_LIB" ]; then RUNTIME_LIB="$L/gmb_runtime_c_build/libgmb_runtime.a"; MIN_LINK=0; fi
if grep -qiE '^[[:space:]]*(FORM|READ|TORCH|TRAIN|JINJA|SCREEN|GRAPHICS|SPRITE|SPRITESET|PSET|PRESET|CIRCLE|PAINT|DRAW|POLYGON|PCOPY|SCREENSET|PALETTE|MOUSE|WINDOW|IMAGE|BITMAP|TILE|FONT|PLOT|CHART|VIDEO)[[:space:](]' "$B" 2>/dev/null; then
  RUNTIME_LIB="$L/gmb_runtime_c_build/libgmb_runtime.a"; MIN_LINK=0
  echo "[c99native] programma con UI/grafica → runtime completo (SDL/Vulkan)"
else
  echo "[c99native] programma testuale → runtime minimo (0 librerie esterne)"
fi
ASRLIB="$L/gmb_runtime_c_build/llmasr_build/libllm_asr.a"
[ -f "$ASRLIB" ] || ASRLIB="$ROOT/native/gmb_runtime_c/build/llmasr_build/libllm_asr.a"
# -dead_strip_dylibs: ld64 tiene SOLO le dylib che servono davvero ai simboli usati dal
# programma. Prima ogni `-l` finiva comunque in LC_LOAD_DYLIB (16 dipendenze per un PRINT).
if [ "$MIN_LINK" = "1" ]; then
  # Link MINIMO: solo runtime minimo + chart (matrici) e le lib di sistema. Niente UI,
  # niente doc/agent/compiler: l'eseguibile che ne esce NON ha librerie esterne.
  # niente TORCH_OBJS/JINJA_OBJ: tirano dentro gfx/UI (e un programma testuale non li usa)
  $CC_BIN $CF $CCFIRST -Wl,-dead_strip_dylibs "$DIR/$CASE.o" -o "$OUTBIN" \
    "$RUNTIME_LIB" "$ROOT/native/gmb_runtime_c/build/gmb_chart_build/libgmb_chart.a" \
    -lpthread -lm -lz -lbz2 -lcurl -lcompression 2>/tmp/gmb_minlink.err || {
      echo "[c99native] link minimo fallito, uso il lib-set completo (vedi /tmp/gmb_minlink.err)" >&2
      MIN_LINK=0
    }
fi
if [ "$MIN_LINK" = "0" ]; then
# Il COMPILATORE C scarica le librerie che gli servono per il link (asset per-file): se una
# delle lib della cache è assente, `gmb_libs_fetch` la prende dal bundle `<libreria>-<rid><ext>`
# e la mette in ~/.gmbasic/libs/<rid>/. Prima del 2026-10-29 il link falliva e basta.
FETCH="$ROOT/native/gmb_runtime_c/build/gmb_libs_fetch"
if [ -x "$FETCH" ]; then
  emit_libs_index "$DIR/work/libs.txt"
  if [ -s "$DIR/work/libs.txt" ]; then
    "$FETCH" "$DIR/work/libs.txt" \
      libcrypto.3.dylib libssl.3.dylib libzstd.1.5.7.dylib libbrotlienc.1.dylib libbrotlidec.1.dylib \
      libpcre2-8.0.dylib libSDL2_mixer.dylib libSDL2_ttf.dylib libSDL2.dylib liblzma.5.dylib \
      liblz4.1.10.0.dylib libMoltenVK.dylib >/dev/null 2>&1 || true
  fi
fi

$CC_BIN $CF $CCFIRST -Wl,-dead_strip_dylibs -Wl,-rpath,@loader_path -Wl,-rpath,"$CODEC" "$DIR/$CASE.o" $TORCH_OBJS $JINJA_OBJ -o "$OUTBIN" \
  "$L/libgmb_vm.a" "$RUNTIME_LIB" "$ASRLIB" \
  "$ROOT/native/gmb_runtime_c/build/gmb_chart_build/libgmb_chart.a" \
  "$ROOT/native/gmb_runtime_c/build/gmbllm_build/libgmbllm.a" \
  "$ROOT/native/gmb_runtime_c/build/llmengine_plugin_build/libllmengine_plugin.a" \
  "$ROOT/native/gmb_runtime_c/build/llmdesc_build/libllmdesc.a" \
  "$CODEC/llmcodec_mlx_build/libllmcodec.a" "$CODEC/llmengine_moe_mlx_build/libllmengine_moe.a" -L"$CODEC" -lllmcodec_ggml -lllmcodec_mlx_affine -lllmcodec_fp8 -Wl,-rpath,"$CODEC" "$L/gmb_compiler_c_build/libgmb_compiler_c.a" -lpthread \
  "$L/gmb_doc_c_build/libgmb_doc.a" "$L/gmb_agent_runtime_build/libgmb_agent_runtime.a" \
  -framework CoreGraphics -framework ApplicationServices -framework ImageIO \
  -framework CoreServices -framework AppKit \
  "$L/gmb_db_c_build/libgmb_db.a" -lm -lcurl -lcompression -lz -lbz2 -lsqlite3 \
  -L"$LIBS_CACHE" -Wl,-rpath,"$LIBS_CACHE" \
  "$LIBS_CACHE/libcrypto.3.dylib" "$LIBS_CACHE/libssl.3.dylib" \
  "$LIBS_CACHE/libzstd.1.5.7.dylib" \
  "$LIBS_CACHE/libbrotlienc.1.dylib" "$LIBS_CACHE/libbrotlidec.1.dylib" \
  "$LIBS_CACHE/libpcre2-8.0.dylib" \
  "$LIBS_CACHE/libSDL2_mixer.dylib" "$LIBS_CACHE/libSDL2_ttf.dylib" "$LIBS_CACHE/libSDL2.dylib" \
  "$LIBS_CACHE/liblzma.5.dylib" "$LIBS_CACHE/liblz4.1.10.0.dylib" "$LIBS_CACHE/libMoltenVK.dylib" \
  -L"$LLMTOKEN_DIR" -lllmtoken -Wl,-rpath,"$LLMTOKEN_DIR" -lz
fi
# ── BUNDLE: il compilato si distribuisce SENZA compilatore ────────────────────────────────────
# Accanto all'eseguibile finiscono i file che gli servono a girare da solo, su una macchina dove
# non esistono né il repo né `shared/settings/`: i settings (`system.settings.json`), l'override
# dell'applicazione (`app.settings.json`, preso accanto al `.bas`) e gli asset (`Resources/`:
# font e sprite). Vale per OGNI build — anche quella senza librerie: le librerie sono opzionali,
# settings e font no. Il runtime li cerca PRIMA nella cartella dell'eseguibile.
emit_program_bundle() { # $1 = eseguibile
  local exe="$1"; [ -f "$exe" ] || return 0
  local out; out="$(cd "$(dirname "$exe")" && pwd)"
  local n=0
  local shared="$ROOT/../shared/settings"
  if [ -f "$shared/system.settings.json" ]; then
    cp -f "$shared/system.settings.json" "$out/system.settings.json" && n=$((n+1))
  fi
  if [ -f "$DIR/app.settings.json" ]; then
    cp -f "$DIR/app.settings.json" "$out/app.settings.json" && n=$((n+1))
  fi
  local res="$ROOT/../Resources"
  if [ -d "$res" ]; then
    mkdir -p "$out/Resources"
    # `cp -p` non va: vogliamo il contenuto, non i timestamp del repo (evita churn inutile).
    for _f in "$res"/*; do
      [ -f "$_f" ] || continue
      cmp -s "$_f" "$out/Resources/$(basename "$_f")" || { cp -f "$_f" "$out/Resources/"; n=$((n+1)); }
    done
  fi
  [ "$n" -gt 0 ] && echo "[c99native] bundle -> $out ($n file: settings + app.settings + Resources)"
  return 0
}

# `<exe>.libs.txt` accanto all'eseguibile: le librerie del manifest da cui DIPENDE (lette dal
# binario prodotto). Serve al runtime C (`gmb_ffi_dl` → `gmb_libs_autoensure`) per scaricare da
# solo ciò che manca nella cache, senza elenchi hardcoded né caricare tutto il gruppo.
if [ -f "$OUTBIN" ]; then
  emit_program_bundle "$OUTBIN"
  emit_exe_libs "$OUTBIN" "${OUTBIN%.dylib}.libs.txt" 2>/dev/null || true
  [ -s "${OUTBIN%.dylib}.libs.txt" ] && echo "[c99native] libs.txt -> ${OUTBIN%.dylib}.libs.txt ($(wc -l < "${OUTBIN%.dylib}.libs.txt" | tr -d ' ') lib)"
fi

# Esegue (CWD = cartella del sample) per un eseguibile; per una libreria si verifica solo
# che i simboli esportati siano presenti.
if grep -q "int main" "$DIR/$CASE.c"; then
  GMB_TORCH_PATH="${GMB_TORCH_PATH:-$HOME/.gmbasic/libs/ml/osx-arm64}"
  (cd "$DIR" && GMB_TORCH_PATH="$GMB_TORCH_PATH" ./work/"$(basename "$CASE")")
else
  # Verifica gli export della libreria (evita pipe con head: con `set -o pipefail` un lettore che
  # chiude presto manda SIGPIPE e farebbe fallire lo script).
  nm -gU "$OUTBIN" > "$DIR/$CASE.syms" 2>/dev/null || true
  LIBPREFIX=$(sed -nE 's/.*static void ([A-Za-z0-9_]+)__ctor.*/\1/p' "$DIR/$CASE.c" | sed -n '1p')
  if [ -n "$LIBPREFIX" ] && grep -q " T _${LIBPREFIX}_init" "$DIR/$CASE.syms"; then
    NE=$(grep -c " T _${LIBPREFIX}_" "$DIR/$CASE.syms" || true)
    echo "LIB ${LIBPREFIX}.dylib export OK (${NE} simboli)"
  else
    echo "LIB: dylib costruita ma export non trovati (prefix='${LIBPREFIX}')"
  fi
fi
