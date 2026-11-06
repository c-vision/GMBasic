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
#   CWD = root del repo. Emette in <dir>/bin/<base>_<suff>.c (output di default del
#   compilatore) e lo esegue; `work/` NON si usa qui (e` per gli artefatti-dati dei sample).
# Serve per la verifica END-TO-END del C99Native (EVAL/&, locali S_eval, ...): il driver
# produce C; qui lo si linka col lib-set completo di gmb_vm_host (runtime+db+agent+doc +
# SDL/OpenSSL/curl della cache utente) e lo si esegue. Prerequisiti: build nativo esistente
# (native/gmb_vm_c/build) e `gmb install --runtime-only`/`--dev`. Esempio: Clipper* LocalVar 7/7.
#
# I FLAG DI COMPILAZIONE non sono fissi: si leggono dalla sezione "compiler" di
# Resources/system.settings.json (default GLOBALI del compilatore), con override
# per-applicazione da <dir>/app.settings.json — stessa semantica documentata in
# common/CommonLib/SettingsRecords.cs (CompilerSettings.Resolve), che resta la fonte
# canonica; questo script la rispecchia per non avere bisogno del CLR.
# Il 3° argomento (cflags) SOSTITUISCE i cflags letti dai settings.
# NESSUNA variabile d'ambiente: i parametri passano da settings o da argomento.
set -euo pipefail

# ── ARGOMENTI DEL PROGRAMMA (dopo `--`) ─────────────────────────────────────────────────────
# Il compilatore principale è quello NATIVO, quindi i tool BASIC (che hanno flag come --check)
# devono poter girare qui: `gmb_c99native_run.sh file.bas out "" -- --check`.
# Si separano PRIMA di leggere $1/$2/$3 (suffisso/cflags), altrimenti `--` finirebbe in SUFF.
PROG_ARGS=()
_ARGS=()
_seen_sep=0
for _a in "$@"; do
  if [ "$_seen_sep" = "0" ] && [ "$_a" = "--" ]; then _seen_sep=1; continue; fi
  if [ "$_seen_sep" = "1" ]; then PROG_ARGS+=("$_a"); else _ARGS+=("$_a"); fi
done
if [ ${#_ARGS[@]} -gt 0 ]; then set -- "${_ARGS[@]}"; else set --; fi

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
# `libs.txt` = indice RIDOTTO per il downloader C: nome<TAB>sha256<TAB>size<TAB>url.
# La costruzione dell'indice vive in `tools/sh/libs_index_lib.sh` (UNA implementazione,
# condivisa col runner dei MiniTest C99Native): qui si dichiara solo DOVE sta la proiezione.
# È la proiezione `shared/settings/libs-index-<rid>.tsv` scritta da `libs_manifest_gh.sh build`:
# la catena C non dipende né da Python né da un parser JSON.
# Serve a DUE cose: (a) il COMPILATORE C che scarica le librerie mancanti prima del link,
# (b) l'eseguibile compilato, che porta accanto `<exe>.libs.txt` e scarica da solo ciò che gli
# serve quando `dlopen` non trova la lib (gmb_ffi_dl → gmb_libs_autoensure).
LIBS_INDEX="$ROOT/../shared/settings/libs-index-$RID.tsv"
. "$TOOLS_DIR/libs_index_lib.sh"

# `emit_exe_libs` / `emit_libs_index` vivono in tools/sh/libs_index_lib.sh (sourced sopra).

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
  # Nome CORRENTE del driver: gmbnative (rename 2026-11-04, AGENTS §nomi-tool).
  # Fallback al nome ritirato gmb_c99native_driver per i tree non ancora rigenerati.
  cmake --build "$BUILD_DIR" --target gmbnative -j4 >/dev/null 2>&1 \
    || cmake --build "$BUILD_DIR" --target gmb_c99native_driver -j4 >/dev/null 2>&1 || true
fi
# RUNTIME C: ricostruzione INCREMENTALE prima del link. Le sorgenti del runtime compaiono in DUE
# build tree (`gmb_runtime_c/build` e `gmb_vm_c/build/gmb_runtime_c_build`) e il link poteva
# prendere la copia del tree VM/C: dopo una modifica al runtime si linkava una lib VECCHIA e il
# sintomo era un `undefined symbol` (classe "librerie native stale"). Qui si ricostruisce il tree
# CANONICO (CMake non fa nulla se è aggiornato, ~0.2s) e il link usa quello.
RT_DIR="$ROOT/native/gmb_runtime_c/build"
if [ -d "$RT_DIR" ]; then
  cmake --build "$RT_DIR" --target gmb_runtime gmb_runtime_min -j4 >/dev/null 2>&1 || true
fi
BIN="tools/gmbnative"
BUILT="native/gmb_compiler_c/build/gmbnative"
# Fallback al nome ritirato (gmb_c99native_driver) SOLO se il nome corrente non esiste:
# il tree condiviso può avere build non ancora rigenerate col rename.
if [ ! -x "$BUILT" ]; then
  BIN="tools/gmb_c99native_driver"
  BUILT="native/gmb_compiler_c/build/gmb_c99native_driver"
fi
if [ -x "$BUILT" ] && { [ ! -x "$BIN" ] || [ "$BUILT" -nt "$BIN" ]; }; then
  BIN="$BUILT"
fi
[ -x "$BIN" ] || { echo "driver assente: $BIN (build tools/)" >&2; exit 2; }
# Profilo C: bootstrap deterministico (i settings del compilatore sono letti dal toolchain C;
# qui niente Python). CF_OVERRIDE permette di forzare i flag.
CF="${CF_OVERRIDE:--O1 -w}"
CC_BIN="cc"

ABS=$(cd "$(dirname "$B")" && pwd)/$(basename "$B")
DIR=$(cd "$(dirname "$B")" && pwd)
mkdir -p "$DIR/bin"
CASE=bin/$(basename "$B" .bas)_${SUFF}


# --no-exe: il driver ora COMPILA E LINKA per default; qui si vuole il SOLO C, perché il
# link lo fa questo script col lib-set completo di gmb_vm_host (runtime+db+agent+doc+SDL).
# Lo stderr del driver non viene più nascosto: "driver fallito" senza il motivo è
# proprio il sintomo che rendeva il driver inutilmente difficile da diagnosticare.
if ! "$BIN" "$ABS" "$DIR/$CASE.c" --no-exe >/dev/null 2>"$DIR/$CASE.drivererr.txt"; then
  # In caso di fallimento il file resta (post-mortem); in caso di successo sparisce:
  # bin/ è già pieno di artefatti, non aggiungiamo rumore a ogni run che va a buon fine.
  { echo "driver fallito:"; sed -n '1,5p' "$DIR/$CASE.drivererr.txt"; } >&2; exit 2
fi
rm -f "$DIR/$CASE.drivererr.txt"

L="$ROOT/native/gmb_vm_c/build"
# Motore/moduli AI = LLMEnv (sorgente unica); i consumer nativi non tengono piu' copie native/gmb_*.
if [ -d "$ROOT/../LLMEnv" ]; then LLMENV="$ROOT/../LLMEnv"; else LLMENV="$ROOT/../../LLMEnv"; fi
INC="-I $ROOT/../shared/include -I $ROOT/native/gmb_runtime_c/include -I $ROOT/native/gmb_runtime_c/src -I $ROOT/native/gmb_chart/include -I $ROOT/native/gmb_compiler_c/include -I $LLMENV/LLMCodec/include -I $ROOT/native/gmb_agent_runtime -I $LLMENV/LLMJinja -I $ROOT/native/gmb_db_c/include -I $ROOT/native/gmb_torch/include -I $ROOT/native/gmb_doc_c/include -I $LLMENV/LLMEngine/moe/include -I $LLMENV/LLMEngine/asr/include -I $LLMENV/LLMEngine/mlx -I $LLMENV/LLMEngine/include -I $LLMENV/LLMEngine/Options -I $ROOT/gmbllm/include -I $ROOT/gmbllm/include/quant -I $ROOT/gmbllm/include/train -I $ROOT/gmbllm/include/lora -I $LLMENV/LLMEngine/plugin -I $LLMENV/LLMDesc -I $LLMENV/LLMToken/include"
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
# Runtime COMPLETO: prima la copia canonica (`gmb_runtime_c/build`, ricostruita sopra), poi —
# solo se manca — quella del tree VM/C. Prima era SEMPRE quella del tree VM/C: dopo una modifica
# al runtime si linkava una lib vecchia ("undefined symbol").
FULL_LIB="$ROOT/native/gmb_runtime_c/build/libgmb_runtime.a"
[ -f "$FULL_LIB" ] || FULL_LIB="$L/gmb_runtime_c_build/libgmb_runtime.a"
RUNTIME_LIB="$MIN_LIB"; MIN_LINK=1
if [ ! -f "$RUNTIME_LIB" ]; then RUNTIME_LIB="$FULL_LIB"; MIN_LINK=0; fi
if grep -qiE '^[[:space:]]*(FORM|READ|TORCH|TRAIN|JINJA|SCREEN|GRAPHICS|SPRITE|SPRITESET|PSET|PRESET|CIRCLE|PAINT|DRAW|POLYGON|PCOPY|SCREENSET|PALETTE|MOUSE|WINDOW|IMAGE|BITMAP|TILE|FONT|PLOT|CHART|VIDEO)[[:space:](]' "$B" 2>/dev/null; then
  RUNTIME_LIB="$FULL_LIB"; MIN_LINK=0
  echo "[c99native] programma con UI/grafica → runtime completo (SDL/Vulkan)" >&2
else
  echo "[c99native] programma testuale → runtime minimo (0 librerie esterne)" >&2
fi
# TRAY: le funzioni gmb_tray_* vivono in una dylib CONDIVISA (target CMake gmb_tray), non
# nella statica del runtime. La si ricostruisce in modo incrementale e la si aggiunge al link
# SOLO se il C generato la usa: un programma testuale senza TRAY resta senza dipendenze.
TRAY_LINK=""
if grep -q 'gmb_tray_' "$DIR/$CASE.c" 2>/dev/null; then
  cmake --build "$ROOT/native/gmb_runtime_c/build" --target gmb_tray -j4 >/dev/null 2>&1 || true
  TRAY_DYLIB="$LIBS_CACHE/libgmb_tray.dylib"
  [ -f "$TRAY_DYLIB" ] || TRAY_DYLIB="$ROOT/build/libs/$RID/libgmb_tray.dylib"
  if [ -f "$TRAY_DYLIB" ]; then
    TRAY_LINK="$TRAY_DYLIB -Wl,-rpath,$(dirname "$TRAY_DYLIB")"
    echo "[c99native] TRAY: link di $(basename "$TRAY_DYLIB")" >&2
  else
    echo "[c99native] TRAY: libgmb_tray.dylib assente (target gmb_tray non costruito)" >&2
  fi
fi
# SPLIT core/AI (2026-11-02): su ELF la dylib `libgmb_runtime_ai.so` è caricata con dlopen e
# risolve a load-time i simboli del core: l'eseguibile deve esportarli (`-rdynamic`).
EXPORTDYN=""
if [ "$(uname -s)" = "Linux" ]; then EXPORTDYN="-Wl,--export-dynamic"; fi
ASRLIB="$L/gmb_runtime_c_build/llmasr_build/libllm_asr.a"
[ -f "$ASRLIB" ] || ASRLIB="$ROOT/native/gmb_runtime_c/build/llmasr_build/libllm_asr.a"
# -dead_strip_dylibs: ld64 tiene SOLO le dylib che servono davvero ai simboli usati dal
# programma. Prima ogni `-l` finiva comunque in LC_LOAD_DYLIB (16 dipendenze per un PRINT).
if [ "$MIN_LINK" = "1" ]; then
  # Link MINIMO: solo runtime minimo + chart (matrici) e le lib di sistema. Niente UI,
  # niente doc/agent/compiler: l'eseguibile che ne esce NON ha librerie esterne.
  # niente TORCH_OBJS/JINJA_OBJ: tirano dentro gfx/UI (e un programma testuale non li usa).
  # LLMDESC: il runtime MINIMO include `gmb_abi.o` (forzato nel link da `gmb_paths.c` via
  # `gmb_abi_link_anchor`) che usa `llmdesc_abi_*` e registra la policy ABI: senza questa
  # statica il link minimo falliva SEMPRE ("_llmdesc_abi_locate non definito") e si ripiegava
  # in silenzio sul lib-set completo — quindi nessun programma testuale usava davvero il minimo.
  LLMDESC_LIB="$ROOT/native/gmb_runtime_c/build/llmdesc_build/libllmdesc.a"
  [ -f "$LLMDESC_LIB" ] || LLMDESC_LIB="$ROOT/native/gmb_vm_c/build/gmb_runtime_c_build/llmdesc_build/libllmdesc.a"
  [ -f "$LLMDESC_LIB" ] || LLMDESC_LIB=""
  $CC_BIN $CF $CCFIRST -Wl,-dead_strip_dylibs $EXPORTDYN "$DIR/$CASE.o" -o "$OUTBIN" \
    "$RUNTIME_LIB" $TRAY_LINK "$ROOT/native/gmb_runtime_c/build/gmb_chart_build/libgmb_chart.a" \
    ${LLMDESC_LIB:+"$LLMDESC_LIB"} \
    -lpthread -lm -lz -lbz2 -lcurl -lcompression 2>/tmp/gmb_minlink.err || {
      echo "[c99native] link minimo fallito, uso il lib-set completo (vedi /tmp/gmb_minlink.err)" >&2
      # Il ripiego deve CAMBIARE davvero runtime: prima RUNTIME_LIB restava la MIN lib, così
      # il "lib-set completo" linkava il runtime minimo e falliva con "Undefined symbols"
      # (caso reale 2026-10-30: `_gmb_msg_delivery_stats` mancante nella min stale) — un
      # messaggio che accusa il programma invece della lib.
      # Il runtime COMPLETO è quello canonico (`gmb_runtime_c/build`, ricostruito sopra):
      # la copia nel tree VM/C resta solo come fallback.
      RUNTIME_LIB="$FULL_LIB"
      MIN_LINK=0
    }
fi
if [ "$MIN_LINK" = "0" ]; then
# Il COMPILATORE C scarica le librerie che gli servono per il link (asset per-file): se una
# delle lib della cache è assente, `gmb_libs_fetch` la prende dal bundle `<libreria>-<rid><ext>`
# e la mette in ~/.gmbasic/libs/<rid>/. Prima del 2026-10-29 il link falliva e basta.
FETCH="$ROOT/native/gmb_runtime_c/build/gmb_libs_fetch"
if [ -x "$FETCH" ]; then
  emit_libs_index "$DIR/bin/libs.txt"
  if [ -s "$DIR/bin/libs.txt" ]; then
    "$FETCH" "$DIR/bin/libs.txt" \
      libcrypto.3.dylib libssl.3.dylib libzstd.1.5.7.dylib libbrotlienc.1.dylib libbrotlidec.1.dylib \
      libpcre2-8.0.dylib libSDL2_mixer.dylib libSDL2_ttf.dylib libSDL2.dylib liblzma.5.dylib \
      liblz4.1.10.0.dylib libMoltenVK.dylib >/dev/null 2>&1 || true
  fi
fi

# Robustezza (2026-11-06): un'altra sessione può rigenerare il build tree mentre questo comando
# gira, lasciando una statica MANCANTE e facendo fallire il link — falso negativo del gate
# (caso reale: 5 MiniTest "FAIL" solo perché `libgmb_doc.a` era in ricostruzione). Se una delle
# statiche attese non c'è, si ricostruisce il target e si prosegue (una volta).
for _t in gmb_doc gmb_agent_runtime; do
  case "$_t" in
    gmb_doc) _needlib="$L/gmb_doc_c_build/libgmb_doc.a" ;;
    *) _needlib="$L/gmb_agent_runtime_build/libgmb_agent_runtime.a" ;;
  esac
  if [ ! -f "$_needlib" ]; then
    echo "[c99native] statica mancante ($_needlib): ricostruisco il target $_t" >&2
    cmake --build "$L" --target "$_t" -j4 >/dev/null 2>&1 || true
  fi
done

$CC_BIN $CF $CCFIRST -Wl,-dead_strip_dylibs $EXPORTDYN -Wl,-rpath,@loader_path -Wl,-rpath,"$CODEC" "$DIR/$CASE.o" $TORCH_OBJS $JINJA_OBJ -o "$OUTBIN" \
  "$L/libgmb_vm.a" "$RUNTIME_LIB" $TRAY_LINK "$ASRLIB" \
  "$ROOT/native/gmb_runtime_c/build/gmb_chart_build/libgmb_chart.a" \
  "$ROOT/native/gmb_runtime_c/build/gmbllm_build/libgmbllm_train.a" \
  "$ROOT/native/gmb_runtime_c/build/gmbllm_build/libgmbllm_core.a" \
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
  # SETTINGS MINIMI: si distribuisce SOLO `app.settings.json`, con le sezioni che il runtime
  # legge (paths/consoleConfiguration/engine) prese dai settings condivisi e sovrascritte da
  # quelle del programma. `system.settings.json` NON si distribuisce: contiene `compiler`,
  # `importFolders`, `defaultNamespaces`… roba del COMPILATORE, non del programma.
  # Settings minimi: UNA regola (CommonLib/ProgramBundle.cs, RuntimeSections) implementata in
  # BASIC — niente Python. Il vecchio tools/data/settings_bundle.py usava `dict | None`
  # (Python 3.10+): su python3 3.9 falliva in SILENZIO (stdout/stderr a /dev/null) e
  # l'eseguibile partiva SENZA settings. Il .bas non ha quel vincolo di versione.
  bundle_tool="$TOOLS_DIR/../basic/settings_bundle.bas"
  if [ -f "$bundle_tool" ]; then
    if command -v gmb >/dev/null 2>&1; then
      gmb "$bundle_tool" --system "$shared/system.settings.json" \
        --app "$DIR/app.settings.json" --out "$out/app.settings.json" >/dev/null && n=$((n+1))
    elif [ -f "$ROOT/../tools/runners/test_ilrun/bin/Debug/net10.0/test_ilrun.dll" ]; then
      dotnet "$ROOT/../tools/runners/test_ilrun/bin/Debug/net10.0/test_ilrun.dll" --engine=il \
        "$bundle_tool" --system "$shared/system.settings.json" \
        --app "$DIR/app.settings.json" --out "$out/app.settings.json" >/dev/null && n=$((n+1))
    fi
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
  # CERVELLI: (a) snapshot del manifest in <out>/settings — il runtime C lo legge PRIMA di
  # quello del checkout, quindi il programma distribuito sa cosa può scaricare; (b) indice
  # `<exe>.brains.txt` con i cervelli DICHIARATI dal programma (`"brains": [...]` nel suo
  # app.settings.json): `gmb_brain_fetch` li porta in <exe>/brains, dove la risoluzione li trova.
  local sharedcfg="$ROOT/../shared/settings"
  local brainbin="$ROOT/native/gmb_runtime_c/build/gmb_brain_fetch"
  if [ -f "$sharedcfg/brains-manifest.json" ]; then
    mkdir -p "$out/settings"
    cmp -s "$sharedcfg/brains-manifest.json" "$out/settings/brains-manifest.json" || {
      cp -f "$sharedcfg/brains-manifest.json" "$out/settings/" && n=$((n+1)); }
  fi
  if [ -f "$DIR/app.settings.json" ] && [ -x "$brainbin" ]; then
    if "$brainbin" --emit-index "$DIR/app.settings.json" "$sharedcfg/brains-manifest.json" "$out/brains.txt" >/dev/null 2>&1 \
       && [ -f "$out/brains.txt" ]; then
      cp -f "$out/brains.txt" "$exe.brains.txt" 2>/dev/null || true
      n=$((n+1))
    fi
  fi
  # CONFIG del programma: il SEED `shared/settings/providers.json` va in `<out>/config/`, dove il
  # runtime lo cerca (`<configDir>` = cartella dell'eseguibile). `AI.PROVIDERS()` lo legge: senza
  # né file né seed la gamba C# rispondeva con un'eccezione e le gambe divergevano.
  # I file dichiarati dal programma (`"config": [...]` nell'app.settings.json) li copre
  # `ProgramBundle` (compilatore C#); qui basta il seed, che è l'unico condiviso.
  local pseed="$ROOT/../shared/settings/providers.json"
  if [ -f "$pseed" ]; then
    mkdir -p "$out/config"
    cmp -s "$pseed" "$out/config/providers.json" || { cp -f "$pseed" "$out/config/"; n=$((n+1)); }
  fi
  [ "$n" -gt 0 ] && echo "[c99native] bundle -> $out ($n file: settings + app.settings + Resources + cervelli + config)" >&2
  return 0
}

# `<exe>.libs.txt` accanto all'eseguibile: le librerie del manifest da cui DIPENDE (lette dal
# binario prodotto). Serve al runtime C (`gmb_ffi_dl` → `gmb_libs_autoensure`) per scaricare da
# solo ciò che manca nella cache, senza elenchi hardcoded né caricare tutto il gruppo.
if [ -f "$OUTBIN" ]; then
  emit_program_bundle "$OUTBIN"
  # FIXTURE dichiarate dal programma (`"fixtures": [...]` in app.settings.json): il COMPILATORE
  # scarica quelle mancanti, SEMPRE in shared/samples/Fixtures (manifest in shared/settings).
  # Best-effort: senza rete si prosegue e il programma fallira' sulla fixture mancante.
  FIXBIN="$ROOT/native/gmb_runtime_c/build/gmb_fixtures"
  if [ -f "$DIR/app.settings.json" ] && [ -x "$FIXBIN" ]; then
    "$FIXBIN" ensure --app "$DIR/app.settings.json" --quiet >/dev/null 2>&1 || true
  fi
  emit_exe_libs "$OUTBIN" "${OUTBIN%.dylib}.libs.txt" "$DIR/$CASE.c" 2>/dev/null || true
  [ -s "${OUTBIN%.dylib}.libs.txt" ] && echo "[c99native] libs.txt -> ${OUTBIN%.dylib}.libs.txt ($(grep -cv '^#' "${OUTBIN%.dylib}.libs.txt" | tr -d ' ') lib)" >&2
fi

# Esegue (CWD = cartella del sample) per un eseguibile; per una libreria si verifica solo
# che i simboli esportati siano presenti.
if grep -q "int main" "$DIR/$CASE.c"; then
  GMB_TORCH_PATH="${GMB_TORCH_PATH:-$HOME/.gmbasic/libs/ml/osx-arm64}"
  (cd "$DIR" && GMB_TORCH_PATH="$GMB_TORCH_PATH" ./bin/"$(basename "$CASE")" ${PROG_ARGS[@]+"${PROG_ARGS[@]}"})
else
  # Verifica gli export della libreria (evita pipe con head: con `set -o pipefail` un lettore che
  # chiude presto manda SIGPIPE e farebbe fallire lo script).
  nm -gU "$OUTBIN" > "$DIR/$CASE.syms" 2>/dev/null || true
  LIBPREFIX=$(sed -nE 's/.*static void ([A-Za-z0-9_]+)__ctor.*/\1/p' "$DIR/$CASE.c" | sed -n '1p')
  if [ -n "$LIBPREFIX" ] && grep -q " T _${LIBPREFIX}_init" "$DIR/$CASE.syms"; then
    NE=$(grep -c " T _${LIBPREFIX}_" "$DIR/$CASE.syms" || true)
    echo "LIB ${LIBPREFIX}.dylib export OK (${NE} simboli)" >&2
  else
    echo "LIB: dylib costruita ma export non trovati (prefix='${LIBPREFIX}')" >&2
  fi
fi
