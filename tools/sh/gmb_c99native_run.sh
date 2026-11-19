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
# DUE COMPILATORI (AGENTS §0, accordo 2026-11-14): su macOS il riferimento è `cc` (Apple
# clang: conosce l'SDK da sé e il suo linker registra TUTTE le dylib caricate — con `zig cc`
# su macOS lo stesso programma usciva con 4 dylib invece di 14 e moriva con SIGSEGV);
# sulle altre piattaforme / cross è `tools/sh/zcc` (`zig cc`: porta la libc del target).
# Va risolto ORA, prima del `cd` in $ROOT più sotto. Override: `--cc <compilatore>`.
SH_TOOLS="$(cd "$(dirname "$0")" && pwd)"
ZCC_DEFAULT=""
if [ "$(uname -s)" = Darwin ]; then
  ZCC_DEFAULT="cc"
elif [ -x "$SH_TOOLS/zcc" ]; then
  ZCC_DEFAULT="$SH_TOOLS/zcc"
fi

# tools/sh/gmb_c99native_run.sh — compila+linka+esegue l'output del driver C99Native.
# Uso: gmb_c99native_run.sh <sample.bas> [suffisso] [cflags]
#      [--cc <compilatore>] [--target <triple>]
#   --cc     il compilatore C. Default: `cc` su macOS (Apple clang, riferimento nativo) e
#            `tools/sh/zcc` (zig cc) sulle altre piattaforme / cross (AGENTS §0: DUE
#            compilatori). `--cc cc` / `--cc tools/sh/zcc` servono anche per la gamba A/B.
#            Accetta anche una stringa multi-parola: `--cc "<zig> cc"`.
#   --rid    RID di destinazione (i sei del piano: osx-arm64, osx-x64, linux-x64, linux-arm64,
#            win-x64, win-arm64). Fissa triple, OS, framework e terze parti via
#            `tools/sh/gmb_link_set.sh` (F5): con `--rid linux-x64` da macOS si PRODUCE il binario
#            Linux e NON lo si esegue (l'esecuzione richiede il RID host).
#   RUNTIME con un RID ≠ host (cross): il runtime dell'host NON è linkabile per un altro RID —
#   è Mach-O/COFF, il linker non ne tira nessun oggetto e dichiara undefined OGNI simbolo del
#   runtime (`gmb_print_stage_end`, `gmb_string_alloc`, `gmb_type_register_udt`, …), un errore
#   che non dice nulla sul codice BASIC. Si prende il runtime costruito PER QUEL RID, in
#   quest'ordine (stessa precedenza del MiniTest):
#     1. la CACHE PER-RID del MiniTest: ${TMPDIR:-/tmp}/gmb_c99native_rt_cache/<rid>/librt.a
#        (la scrive `tools/sh/test_minitests_c99native`, invalidata da uno stamp di contenuto;
#         la cartella è per RID perché un run host non deve sovrascrivere quella del cross; qui
#         `librt.a` è l'archivio del compilatore CANONICO (zig) — il runner tiene un archivio per
#         compilatore (`librt.<tag>.a` per la gamba A/B, che non tocca il canonico), così
#         alternare zig e cc non ricostruisce il runtime a ogni cambio);
#     2. l'ARCHIVIO DEL BUILD CROSS: <monorepo>/build/libs/<rid>/libgmb_runtime.a
#        (`tools/sh/libs_cross_build.sh <rid> --zig <zig>`).
#   Serve produrne UNA delle due UNA VOLTA SOLA per RID: poi ogni caso cross si costruisce allo
#   stesso modo di un caso host e si esegue col comando Docker che lo script stampa (per Linux;
#   un PE32+ richiede Windows/Wine). Se non esiste né la cache né l'archivio, lo script esce 2
#   elencando i due comandi che li producono, invece di lasciare decine di undefined a valle.
#   Run host (nessun `--rid`, o il RID dell'host): percorso invariato, runtime canonico di
#   `native/gmb_runtime_c/build` con il suo `cmake --build` incrementale.
#   --target triple di destinazione (default: host), es. aarch64-linux-musl / x86_64-apple-darwin.
#            I flag dipendenti dalla piattaforma (-arch, -dead_strip_dylibs, @loader_path,
#            dynamic_lookup, estensione delle librerie) si derivano da qui: nessuno e' piu' cablato
#            su arm64/Darwin.
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
# ── COMPILATORE E TARGET (argomenti, non variabili d'ambiente) ──────────────────────────
# Si estraggono PRIMA dei posizionali, cosi' restano validi <file> [suffisso] [cflags].
CC_ARG=""
TARGET_TRIPLE=""
RID_ARG=""
STATIC_ARG=0
_POS=()
_i=0
while [ "$_i" -lt "${#_ARGS[@]}" ]; do
  _a="${_ARGS[$_i]}"
  case "$_a" in
    --cc)       CC_ARG="${_ARGS[$((_i+1))]:-}"; _i=$((_i+2)); continue ;;
    --cc=*)     CC_ARG="${_a#--cc=}"; _i=$((_i+1)); continue ;;
    --target)   TARGET_TRIPLE="${_ARGS[$((_i+1))]:-}"; _i=$((_i+2)); continue ;;
    --target=*) TARGET_TRIPLE="${_a#--target=}"; _i=$((_i+1)); continue ;;
    # --rid: F5. Il RID (i sei del piano) determina triple, OS, framework e terze parti.
    # Senza --rid resta l'host. Se c'è anche --target, vince --target (è più specifico).
    --rid)      RID_ARG="${_ARGS[$((_i+1))]:-}"; _i=$((_i+2)); continue ;;
    --rid=*)    RID_ARG="${_a#--rid=}"; _i=$((_i+1)); continue ;;
    # --static: binario a FILE SINGOLO per Linux (musl, `-static`): nessuna libreria dinamica,
    # nessun rpath, nessun SDK. Le terze parti e il runtime si prendono dagli archivi STATICI
    # della cache del RID (`~/.gmbasic/libs/<rid>/lib/*.a`). Solo Linux: su Darwin/Windows
    # `-static` non esiste in questa forma (si dice e si esce).
    --static)   STATIC_ARG=1; _i=$((_i+1)); continue ;;
  esac
  _POS+=("$_a"); _i=$((_i+1))
done
if [ ${#_POS[@]} -gt 0 ]; then set -- "${_POS[@]}"; else set --; fi
# --static implica il target musl: senza un `--target`/`--rid` esplicito si parte da linux-x64
# (l'unico RID Linux con artefatti locali, AGENTS §Toolchain).
if [ "$STATIC_ARG" = 1 ] && [ -z "$TARGET_TRIPLE" ]; then
  case "${RID_ARG:-linux-x64}" in
    linux-arm64) TARGET_TRIPLE="aarch64-linux-musl" ;;
    linux-x64)   TARGET_TRIPLE="x86_64-linux-musl" ;;
    *)           TARGET_TRIPLE="x86_64-linux-musl" ;;
  esac
fi

ROOT=$(cd "$(dirname "$0")/../../GMBasic" && pwd)
TOOLS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"   # tools/ (script ausiliari)
# ── F5: `--rid` fissa la triple. Il mapping RID→triple vive SOLO in `gmb_link_set.sh` (§17.1):
# duplicarlo qui lo farebbe divergere al primo RID nuovo. `--target` resta più specifico.
if [ -n "$RID_ARG" ] && [ -z "$TARGET_TRIPLE" ]; then
  TARGET_TRIPLE="$(bash "$TOOLS_DIR/gmb_link_set.sh" --rid "$RID_ARG" --print triple)" || exit 2
fi
# Cache ON-DEMAND delle librerie native (CommonLib.LibsResolver / `gmb libs ensure`): il link usa
# le copie della CACHE, che hanno install-name `@rpath/...`, NON i dist di build (possono essere
# stale e avere `@loader_path`: caso reale `libllmtoken` @loader_path → l'eseguibile non partiva
# con "Library not loaded: @loader_path/libllmtoken.dylib").
# ── PIATTAFORMA DI DESTINAZIONE ─────────────────────────────────────────────────────────
# Default: l'host. Con `--target <triple>` si derivano OS e architettura dal triple, e da
# quelli TUTTI i flag dipendenti dalla piattaforma (prima `-arch arm64` era cablato e il link
# usava `-dead_strip_dylibs`/`@loader_path` anche dove non esistono).
TARGET_OS="$(uname -s)"
TARGET_ARCH="$(uname -m)"
case "$TARGET_ARCH" in aarch64|arm64) TARGET_ARCH=arm64 ;; x86_64|amd64) TARGET_ARCH=x86_64 ;; esac
if [ -n "$TARGET_TRIPLE" ]; then
  case "$TARGET_TRIPLE" in
    aarch64-*|arm64-*) TARGET_ARCH=arm64 ;;
    x86_64-*|amd64-*)  TARGET_ARCH=x86_64 ;;
    i386-*|i686-*)     TARGET_ARCH=i686 ;;
  esac
  case "$TARGET_TRIPLE" in
    *-darwin*|*-macos*|*-apple*) TARGET_OS=Darwin ;;
    *-linux*)                    TARGET_OS=Linux ;;
    *-windows*|*mingw*)          TARGET_OS=Windows ;;
  esac
fi
case "$TARGET_OS-$TARGET_ARCH" in
  Darwin-arm64)   RID=osx-arm64 ;;
  Darwin-x86_64)  RID=osx-x64 ;;
  Linux-x86_64)   RID=linux-x64 ;;
  Linux-arm64)    RID=linux-arm64 ;;
  Windows-x86_64) RID=win-x64 ;;
  Windows-arm64)  RID=win-arm64 ;;
  *)              RID=osx-arm64 ;;
esac
# ── F5: IL SET DI LINK DEL RID, da `gmb_link_set.sh` (una sola fonte) ────────────────────────────
# Framework, librerie di sistema e terze parti non sono più cablati su Darwin/arm64: `LS_*`
# (array inclusi) è quello che il RID dichiara. `LS_FRAMEWORKS` è vuoto fuori da macOS,
# `LS_SYS` non contiene `-lcompression` fuori da macOS, `LS_THIRD` dà `-L<cache> -l<nome>` con
# l'estensione scelta dal linker, `LS_AGENT=0` fuori da Darwin (`libgmb_agent_runtime` usa AX/CG).
eval "$(bash "$TOOLS_DIR/gmb_link_set.sh" --rid "$RID" --cache "$HOME/.gmbasic/libs/$RID")" || exit 2
# Flag per piattaforma: UNA sola fonte. Vuoto = il flag non esiste su quel target.
# Il triple va PASSATO al compilatore: senza, `--target` cambierebbe solo i flag di link e la
# compilazione resterebbe sull'host (verificato: un cross dichiarato compilava nativo).
ARCHFLAG=""        # Darwin: -arch <arch>; altrove il target lo sceglie il compilatore
TARGETFLAG=""
# `-isysroot`: senza, zig non trova le lib di sistema dei target Apple (piano L13).
if [ -n "$TARGET_TRIPLE" ]; then TARGETFLAG="-target $TARGET_TRIPLE ${LS_SDK[*]+${LS_SDK[*]}}"; fi
# I flag di link arrivano dal resolver (F5); qui restano i nomi.
STRIPDYLIB="$LS_STRIP"
RPATH_LOADER="$LS_RPATH"
UNDEFSYM="$LS_UNDEF"
DYLIBEXT="$LS_DYLIBEXT"
NM_GU="$LS_NM_GU"
[ "$TARGET_OS" = Darwin ] && ARCHFLAG="-arch $TARGET_ARCH"
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
# Compilatore C: default ZIG (tools/sh/zcc); `--cc cc` per la gamba A/B, o "<zig> cc" per il cross.
CC_BIN="${CC_ARG:-${ZCC_DEFAULT:-cc}}"

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
  $CC_BIN $TARGETFLAG $CF $INC -c "$ROOT/native/gmb_torch/src/gmb_torch.c" -o "$TMPOBJ/gmb_torch.o"
  TORCH_OBJS="$TMPOBJ/gmb_torch.o"
  if [ -f "$ROOT/native/gmb_torch/src/gmb_torch_str.c" ]; then
    $CC_BIN $TARGETFLAG $CF $INC -c "$ROOT/native/gmb_torch/src/gmb_torch_str.c" -o "$TMPOBJ/gmb_torch_str.o"
    TORCH_OBJS="$TORCH_OBJS $TMPOBJ/gmb_torch_str.o"
  fi
fi
# MOTORE 3D (P2): i moduli gmb_3d_* del runtime chiedono la suite grafica minima
# (gm_gfx_raster + gm_gfx_sector), esattamente come il target gmb_3d_oracle; nell'archivio
# statico quei simboli restano VOLUTAMENTE non risolti finché qualcuno non li usa. Si
# compilano qui, e SOLO per i programmi che chiamano la facciata gmb_world_ o il motore
# gmb_3d_*: gli altri restano con il link di sempre.
GFX_OBJS=""
if grep -q "gmb_world_\|gmb_3d_" "$DIR/$CASE.c"; then
  GFX_SUITE="$ROOT/../shared/libs/graphics"
  GFX_INC="-I $GFX_SUITE/include -I $ROOT/../shared/libs/math/include -I $ROOT/../shared/libs/compute/include"
  for _g in gm_gfx_raster gm_gfx_sector; do
    if ! $CC_BIN $TARGETFLAG $CF $INC $GFX_INC -c "$GFX_SUITE/src/$_g.c" -o "$TMPOBJ/$_g.o"; then
      echo "[c99native] suite 3D: compilazione di $_g.c fallita" >&2
      exit 5
    fi
    GFX_OBJS="$GFX_OBJS $TMPOBJ/$_g.o"
  done
fi
# `-w` sul .c GENERATO (convenzione gate C99 + test_minitests_c99native): i warning
# (es. -Wunused-value delle statement-expression) inquinerebbero lo stdout catturato
# dagli harness (falso DIFF di parity4). Gli errori restano fatali (exit!=0).
$CC_BIN $TARGETFLAG $CF $INC -c "$DIR/$CASE.c" -o "$DIR/$CASE.o"
# CODEC (progetto autonomo gmb_ggml_codec): il CORE è la static lib (contenitori+registry);
# le IMPLEMENTAZIONI sono plugin DYLIB che il runtime linka (come il target CMake gmb_runtime:
# gmb_codec_ggml/mlx_affine/fp8). Senza questi, i simboli del codec (gmb_iq2_xxs_quantize,
# gmb_moe_affine_*, gmb_ggml_type_of_fmt, gmb_gguf_write) restano non definiti. Fix 2026-09-24.
CODEC="$LLMENV/LLMEngine/mlx/build-gmbasic"   # codec (dylib/a) nel build MLX di LLMEnv (post-migrazione)
# LIBRARY: il C generato non ha `int main` -> si produce una libreria condivisa (.gmblib),
# non un eseguibile; niente esecuzione (nessun entry point).
OUTBIN="$DIR/$CASE"; CCFIRST="$TARGETFLAG $ARCHFLAG"
if ! grep -q "int main" "$DIR/$CASE.c"; then OUTBIN="$DIR/$CASE.$DYLIBEXT"; CCFIRST="-shared -fPIC $UNDEFSYM $TARGETFLAG $ARCHFLAG"; fi
# libllmtoken: dalla cache (install-name @rpath). Fallback al BUILD di LLMEnv, mai al dist di
# staging (variante @loader_path che non si risolve accanto all'eseguibile).
LLMTOKEN_DIR="$LIBS_CACHE"
[ -f "$LLMTOKEN_DIR/libllmtoken.$DYLIBEXT" ] || LLMTOKEN_DIR="$LLMENV/build/LLMToken"
# ── Quale runtime? ────────────────────────────────────────────────────────────────────────────
# SEMPRE il runtime COMPLETO. Il ramo «programma testuale → gmb_runtime_min» è stato RIMOSSO
# (2026-11-12): quel lib-set NON è linkabile, perché gli oggetti del runtime minimo chiedono
# sqlite3, lzma, zstd, brotli, OpenSSL, lz4 e simboli gfx/TUI/brain che non gli appartengono.
# Con ld64 il link falliva e lo script ripiegava IN SILENZIO sul lib-set completo, quindi il
# minimo non è mai stato usato nei fatti; con lld di `zig cc` lo stesso link «riesce» e produce
# un eseguibile con gli stub a 0, che muore al primo costruttore (EXC_BAD_ACCESS a 0x0) —
# misurato il 2026-11-12 su llm_tokens_error.bas. Un solo link, quello che funziona: nessun
# ripiego muto e nessun eseguibile diverso a seconda del linker.
# RID dell'HOST: serve QUI (non solo in fondo) perché un cross non può linkare il runtime
# dell'host. Era calcolato ~200 righe più giù, dopo la scelta del runtime.
case "$(uname -s)-$(uname -m)" in
  Darwin-arm64)                  HOST_RID=osx-arm64 ;;
  Darwin-x86_64)                 HOST_RID=osx-x64 ;;
  Linux-x86_64)                  HOST_RID=linux-x64 ;;
  Linux-aarch64|Linux-arm64)     HOST_RID=linux-arm64 ;;
  *)                             HOST_RID=osx-arm64 ;;
esac
FULL_LIB="$ROOT/native/gmb_runtime_c/build/libgmb_runtime.a"
[ -f "$FULL_LIB" ] || FULL_LIB="$L/gmb_runtime_c_build/libgmb_runtime.a"
RUNTIME_LIB="$FULL_LIB"
# CROSS (F5): il runtime dell'host NON è linkabile per un altro RID — è Mach-O, lld non ne tira
# nessun oggetto e il linker dichiara undefined OGNI simbolo del runtime (`gmb_print_stage_end`,
# `gmb_string_alloc`, `gmb_type_register_udt`, `gmb_print_double`, …), un errore che non dice
# nulla sul codice BASIC in prova. Si usa il runtime costruito PER QUEL RID, con la stessa
# precedenza del MiniTest: prima la sua cache per-RID, poi l'archivio del build cross.
if [ "$RID" != "$HOST_RID" ]; then
  MONO_DIR="$(cd "$ROOT/.." && pwd)"
  # Stessa cartella del MiniTest (`test_minitests_c99native`): un runtime per RID, così un run
  # dell'host non può sovrascrivere per sbaglio quello del cross (bug misurato 2026-11-12).
  RT_CACHE="${TMPDIR:-/tmp}/gmb_c99native_rt_cache/$RID/librt.a"
  CROSS_LIB="$MONO_DIR/build/libs/$RID/libgmb_runtime.a"
  if [ -f "$RT_CACHE" ]; then
    RUNTIME_LIB="$RT_CACHE"
    echo "[c99native] runtime $RID: cache per-RID ($RT_CACHE)" >&2
  elif [ -f "$CROSS_LIB" ]; then
    RUNTIME_LIB="$CROSS_LIB"
    echo "[c99native] runtime $RID: archivio del build cross ($CROSS_LIB)" >&2
  else
    echo "[c99native] runtime $RID ASSENTE: né la cache per-RID né l'archivio del build cross." >&2
    echo "  Un cross non può linkare il runtime dell'host ($HOST_RID): senza, il link dichiara" >&2
    echo "  undefined ogni gmb_* del runtime. Una delle due, una volta sola:" >&2
    echo "    tools/sh/libs_cross_build.sh $RID --zig <zig>   # archivio CMake per il RID" >&2
    echo "    GMB_MINI_ONLY=\"<dominio>/<file>.bas\" tools/sh/test_minitests_c99native --rid $RID  # cache per-RID" >&2
    exit 2
  fi
fi
# TRAY: le funzioni gmb_tray_* vivono in una dylib CONDIVISA (target CMake gmb_tray), non
# nella statica del runtime. La si ricostruisce in modo incrementale e la si aggiunge al link
# SOLO se il C generato la usa: un programma testuale senza TRAY resta senza dipendenze.
TRAY_LINK=""
if grep -q 'gmb_tray_' "$DIR/$CASE.c" 2>/dev/null; then
  cmake --build "$ROOT/native/gmb_runtime_c/build" --target gmb_tray -j4 >/dev/null 2>&1 || true
  TRAY_DYLIB="$LIBS_CACHE/libgmb_tray.$DYLIBEXT"
  [ -f "$TRAY_DYLIB" ] || TRAY_DYLIB="$ROOT/build/libs/$RID/libgmb_tray.$DYLIBEXT"
  if [ -f "$TRAY_DYLIB" ]; then
    TRAY_LINK="$TRAY_DYLIB -Wl,-rpath,$(dirname "$TRAY_DYLIB")"
    echo "[c99native] TRAY: link di $(basename "$TRAY_DYLIB")" >&2
  else
    echo "[c99native] TRAY: libgmb_tray.dylib assente (target gmb_tray non costruito)" >&2
  fi
fi
# SPLIT core/AI: le nostre dylib risolvono a LOAD-TIME i simboli del core (import
# `<flat-namespace>`, linkate con `-undefined dynamic_lookup`). Su ELF l'eseguibile deve
# esportarli (`--export-dynamic`). Su Darwin ld64 esporta i globali dell'eseguibile di default,
# lld di `zig cc` NO: dead-strippa ciò che nulla referenzia — misurato il 2026-11-12, stesso
# link: clang 4020 simboli esportati (0 su 35 assenti), zig 198 con **32 su 35** assenti, cioè
# stub a 0 e crash nei percorsi AI. `-export_dynamic` lo accetta solo ld64 (`--export-dynamic`
# solo lld) e in lld non cambia gli export: si usa invece `-u,_simbolo` per OGNI simbolo cercato
# a load-time, che è ADDITIVO (non una lista chiusa) e supportato da entrambi i linker.
# QUALI simboli si forzano: SOLO l'ABI minima (`libs_exports_file.sh abi`, 4 simboli), cioè
# `gmb_abi_set_llmdesc` — chiamato dal COSTRUTTORE della dylib — più i tre target del `dlsym`.
#
# PERCHÉ NON TUTTI GLI IMPORT DELLE DYLIB (misurato 2026-11-12). `-u` non "esporta" un simbolo:
# obbliga il linker a TENERE l'oggetto che lo definisce. Forzare l'unione degli import di tutte le
# dylib (AI, LLM, fonts, grafica) faceva entrare quel codice in OGNI programma: su `PRINT "ciao"`
# il binario passava da 198 KB a 594 KB, con 604 export al posto di 189 e oggetti `gmb_ai_*` mai
# chiamati dal BASIC. Con l'anchor in `gmb_abi.c` (gmb_abi_set_llmdesc referenziato dal core) lld
# tiene ed esporta da sé ciò che le dylib si aspettano: i sette MiniTest AI passano con zig senza
# forzare nulla, mentre i 4 simboli qui sotto coprono il caso del bug originale (costruttore).
EXPORTDYN=""
EXPORT_SYMS_FILE=""
_EXPORT_SYMS_TMP=0
case "$TARGET_OS" in
  Linux)
    EXPORTDYN="-Wl,--export-dynamic" ;;
  Darwin)
    if command -v dyld_info >/dev/null 2>&1; then
      EXPORT_SYMS_FILE="$(mktemp "${TMPDIR:-/tmp}/gmbexports.XXXXXX")"
      _EXPORT_SYMS_TMP=1
      bash "$TOOLS_DIR/libs_exports_file.sh" abi > "$EXPORT_SYMS_FILE"
      while IFS= read -r _s; do EXPORTDYN="$EXPORTDYN -Wl,-u,$_s"; done < "$EXPORT_SYMS_FILE"
    else
      echo "[c99native] dyld_info assente: export dell'ABI minima (llmdesc) non forzato" >&2
    fi
    ;;
esac
ASRLIB="$L/gmb_runtime_c_build/llmasr_build/libllm_asr.a"
[ -f "$ASRLIB" ] || ASRLIB="$ROOT/native/gmb_runtime_c/build/llmasr_build/libllm_asr.a"
# -dead_strip_dylibs: ld64 tiene SOLO le dylib che servono davvero ai simboli usati dal
# programma. Prima ogni `-l` finiva comunque in LC_LOAD_DYLIB (16 dipendenze per un PRINT).
# Il COMPILATORE C scarica le librerie che gli servono per il link (asset per-file): se una
# delle lib della cache è assente, `gmb_libs_fetch` la prende dal bundle `<libreria>-<rid><ext>`
# e la mette in ~/.gmbasic/libs/<rid>/. Prima del 2026-10-29 il link falliva e basta.
FETCH="$ROOT/native/gmb_runtime_c/build/gmb_libs_fetch"
if [ -x "$FETCH" ]; then
  emit_libs_index "$DIR/bin/libs.txt"
  if [ -s "$DIR/bin/libs.txt" ] && [ ${#LS_FETCH_NAMES[@]} -gt 0 ]; then
    # Nomi ESATTI letti dall'indice del RID (`libcrypto.3.dylib` su macOS, `libcrypto.so.3` su
    # Linux): prima erano cablati con l'estensione `.dylib` e su un altro RID non esistevano.
    "$FETCH" "$DIR/bin/libs.txt" ${LS_FETCH_NAMES[@]+"${LS_FETCH_NAMES[@]}"} >/dev/null 2>&1 || true
  fi
fi

# Robustezza (2026-11-06): un'altra sessione può rigenerare il build tree mentre questo comando
# gira, lasciando una statica MANCANTE e facendo fallire il link — falso negativo del gate
# (caso reale: 5 MiniTest "FAIL" solo perché `libgmb_doc.a` era in ricostruzione). Se una delle
# statiche attese non c'è, si ricostruisce il target e si prosegue (una volta).
# `gmb_agent_runtime` ha senso solo dove il suo archivio è linkabile (Darwin: AX/CG).
_ENSURE_TARGETS="gmb_doc"
[ "$LS_AGENT" = 1 ] && _ENSURE_TARGETS="gmb_doc gmb_agent_runtime"
for _t in $_ENSURE_TARGETS; do
  case "$_t" in
    gmb_doc) _needlib="$L/gmb_doc_c_build/libgmb_doc.a" ;;
    *) _needlib="$L/gmb_agent_runtime_build/libgmb_agent_runtime.a" ;;
  esac
  if [ ! -f "$_needlib" ]; then
    echo "[c99native] statica mancante ($_needlib): ricostruisco il target $_t" >&2
    cmake --build "$L" --target "$_t" -j4 >/dev/null 2>&1 || true
  fi
done

# Set di link dal resolver (F5): `LS_FRAMEWORKS` è vuoto fuori da macOS (niente `-framework` su
# Linux/Windows), `LS_SYS` non contiene `-lcompression` fuori da macOS, `LS_THIRD` è
# `-L<cache> -l<crypto|ssl|…>` con l'estensione che sceglie il linker.
AGENT_LIB=""
if [ "$LS_AGENT" = 1 ] && [ -f "$L/gmb_agent_runtime_build/libgmb_agent_runtime.a" ]; then
  AGENT_LIB="$L/gmb_agent_runtime_build/libgmb_agent_runtime.a"
fi
# LLM dell'host (`llmtoken`, `llmcodec_*`, gmbllm/llmengine_plugin/llmdesc): esistono solo per il
# RID nativo (`LS_LLM`). Per un RID diverso sono Mach-O/COFF dell'host e il loro errore
# («not a mach-o file», «unable to find -lllmtoken») nasconde quello vero: si omettono e si dice.
LLM_LINK=()
if [ "$LS_LLM" = 1 ]; then
  LLM_LINK=(-Wl,-rpath,"$CODEC" "$ASRLIB" \
    "$ROOT/native/gmb_runtime_c/build/gmbllm_build/libgmbllm_train.a" \
    "$ROOT/native/gmb_runtime_c/build/gmbllm_build/libgmbllm_core.a" \
    "$ROOT/native/gmb_runtime_c/build/llmengine_plugin_build/libllmengine_plugin.a" \
    "$ROOT/native/gmb_runtime_c/build/llmdesc_build/libllmdesc.a" \
    "$CODEC/llmcodec_mlx_build/libllmcodec.a" "$CODEC/llmengine_moe_mlx_build/libllmengine_moe.a" \
    -L"$CODEC" -lllmcodec_ggml -lllmcodec_mlx_affine -lllmcodec_fp8 -Wl,-rpath,"$CODEC" \
    -L"$LLMTOKEN_DIR" -lllmtoken -Wl,-rpath,"$LLMTOKEN_DIR")
else
  echo "[c99native] llmtoken/llmcodec_* omesse per $RID: la pila LLM e' costruita per $LS_HOST_RID (LS_LLM=0)" >&2
fi
if [ "$STATIC_ARG" = 1 ]; then
  # ── LINK STATICO (musl) ────────────────────────────────────────────────────────────────
  # Binario a FILE SINGOLO: nessuna dylib/.so, nessun rpath, nessun SDK. Si linkano SOLO gli
  # archivi statici della cache del RID (`~/.gmbasic/libs/<rid>/lib/*.a`): le librerie
  # dell'host (`$L/*.a`: Mach-O/COFF) non sono linkabili per un altro target.
  #
  # Runtime HEADLESS: `core` + `sql` della cache (NON `ui`, che porta SDL). Il runtime
  # completo trascina SDL e, a cascata, bzip2/png/libsamplerate, che nella cache esistono
  # solo come .so: un binario a file singolo è per programmi console/servizio, quindi si
  # linka la variante senza UI. Un programma che disegna non è staticizzabile oggi: il link
  # lo dice con i simboli mancanti (nessun successo silenzioso).
  CACHE_LIB="$HOME/.gmbasic/libs/$RID/lib"
  MONO_DIR="$(cd "$ROOT/.." && pwd)"
  STATIC_RT=()
  # Ordine: archivio HEADLESS appena cross-compilato dal checkout (`build/libs/<rid>/libgmb_runtime.a`
  # = core+sql, senza UI/SDL) → poi la cache del RID. La variante con UI trascinerebbe SDL2 e la
  # catena X11/Wayland/ALSA/pulse/harfbuzz (488 simboli, solo .so): non è staticizzabile.
  if [ -f "$MONO_DIR/build/libs/$RID/libgmb_runtime.a" ]; then
    STATIC_RT=("$MONO_DIR/build/libs/$RID/libgmb_runtime.a")
    [ -f "$MONO_DIR/build/libs/$RID/libgmb_chart.a" ] && STATIC_RT+=("$MONO_DIR/build/libs/$RID/libgmb_chart.a")
  elif [ -f "$CACHE_LIB/libgmb_runtime_core.a" ] && [ -f "$CACHE_LIB/libgmb_runtime_sql.a" ]; then
    STATIC_RT=("$CACHE_LIB/libgmb_runtime_core.a" "$CACHE_LIB/libgmb_runtime_sql.a")
  elif [ -f "$RUNTIME_LIB" ]; then
    STATIC_RT=("$RUNTIME_LIB")
  fi
  if [ ${#STATIC_RT[@]} -eq 0 ]; then
    echo "[c99native] --static: runtime per $RID assente (né core/sql in <cache>/lib né cache per-RID)." >&2
    echo "  Popolali con: gmb libs ensure --rid $RID" >&2
    exit 2
  fi
  STATIC_OWN=()
  for _a in libgmb_compiler_c libgmb_chart libgmb_db; do
    [ -f "$CACHE_LIB/$_a.a" ] && STATIC_OWN+=("$CACHE_LIB/$_a.a")
  done
  STATIC_THIRD=()
  # Solo ciò che il runtime headless usa davvero; le terze parti grafiche (SDL2/ttf/freetype)
  # restano fuori perché la variante `core` non le referenzia.
  for _l in z zstd lzma pcre2-8 brotlidec brotlienc brotlicommon sqlite3 ssl crypto curl; do
    [ -f "$CACHE_LIB/lib$_l.a" ] && STATIC_THIRD+=("-l$_l")
  done
  if [ ! -f "$CACHE_LIB/libz.a" ]; then
    echo "[c99native] --static: manca $CACHE_LIB/libz.a (archivi statici del RID assenti)." >&2
    echo "  Popolali con: gmb libs ensure --rid $RID   (o tools/sh/libs_cross_build.sh $RID --zig <zig>)" >&2
    exit 2
  fi
  if ! $CC_BIN $CF $TARGETFLAG -static -O2 -w "$DIR/$CASE.o" \
       "$TOOLS_DIR/../c/musl_fortify_compat.c" -o "$OUTBIN" \
       ${STATIC_RT[@]+"${STATIC_RT[@]}"} ${STATIC_OWN[@]+"${STATIC_OWN[@]}"} $GFX_OBJS \
       -L"$CACHE_LIB" ${STATIC_THIRD[@]+"${STATIC_THIRD[@]}"} -lm -lpthread 2>"$DIR/static_link.err"; then
    echo "[c99native] --static: link fallito (vedi $DIR/static_link.err)." >&2
    echo "  Il link statico usa SOLO gli archivi di $CACHE_LIB: un simbolo che vive in una" >&2
    echo "  libreria non presente in forma statica non è linkabile a file singolo." >&2
    tail -8 "$DIR/static_link.err" >&2
    exit 5
  fi
  if command -v file >/dev/null 2>&1; then
    if ! file "$OUTBIN" | grep -q "statically linked"; then
      echo "[c99native] --static: il binario NON risulta statico: $(file -b "$OUTBIN")" >&2
      exit 5
    fi
  fi
  echo "[c99native] link STATICO ($TARGET_TRIPLE, musl): $(basename "$OUTBIN")" >&2
else
$CC_BIN $CF $CCFIRST $STRIPDYLIB $EXPORTDYN $RPATH_LOADER "$DIR/$CASE.o" $TORCH_OBJS $JINJA_OBJ $GFX_OBJS -o "$OUTBIN" \
  "$L/libgmb_vm.a" "$RUNTIME_LIB" $TRAY_LINK \
  ${LLM_LINK[@]+"${LLM_LINK[@]}"} \
  "$ROOT/native/gmb_runtime_c/build/gmb_chart_build/libgmb_chart.a" \
  "$L/gmb_compiler_c_build/libgmb_compiler_c.a" -lpthread \
  "$L/gmb_doc_c_build/libgmb_doc.a" $AGENT_LIB \
  ${LS_FRAMEWORKS[@]+"${LS_FRAMEWORKS[@]}"} \
  "$L/gmb_db_c_build/libgmb_db.a" -lm ${LS_SYS[@]+"${LS_SYS[@]}"} \
  -Wl,-rpath,"$LIBS_CACHE" ${LS_THIRD[@]+"${LS_THIRD[@]}"} \
  -lz
fi
# ── GATE EXPORT: il link può "riuscire" e produrre un binario che crasha all'avvio ────────────
# Per le dylib conta ciò che l'eseguibile ESPORTA. Con lld un simbolo che nulla referenzia non è
# un errore di link: la dylib ha uno stub NULL e ci salta al primo uso (EXC_BAD_ACCESS at 0x0 in un
# inizializzatore, prima di `main`, zero output). Qui si verifica SUBITO DOPO il link, prima di
# eseguire: se resta un simbolo scoperto si dice quale e si esce in errore, cancellando il binario.
if [ "$TARGET_OS" = Darwin ] && [ -n "$EXPORT_SYMS_FILE" ] && [ -s "$EXPORT_SYMS_FILE" ]; then
  if ! bash "$TOOLS_DIR/libs_exports_file.sh" verify "$OUTBIN" "$EXPORT_SYMS_FILE"; then
    rm -f "$OUTBIN"
    [ "$_EXPORT_SYMS_TMP" = 1 ] && rm -f "$EXPORT_SYMS_FILE"
    echo "[c99native] ERRORE: link riuscito ma eseguibile INCOMPLETO — i simboli qui sopra sono cercati" >&2
    echo "  a load-time dalle nostre dylib e non sono esportati: il binario sarebbe crashato all'avvio" >&2
    echo "  (stub NULL). Ricompilare le dylib/bundle (tools/sh/libs_build_release.sh) e ritentare." >&2
    exit 1
  fi
fi
[ "$_EXPORT_SYMS_TMP" = 1 ] && rm -f "$EXPORT_SYMS_FILE"
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
      # `--` PRIMA degli argomenti del tool: senza, il CLI li interpreta come OPZIONI di `run`
      # e muore con "gmb: opzione sconosciuta '--system'" (il bundle settings veniva saltato
      # in silenzio, quindi l'eseguibile distribuito partiva senza app.settings.json).
      # Contratto: `gmb <file.bas> -- ARG...` inoltra ARG al PROGRAMMA (vedi gmb_cli_usage).
      gmb "$bundle_tool" -- --system "$shared/system.settings.json" \
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
  emit_exe_libs "$OUTBIN" "${OUTBIN%.$DYLIBEXT}.libs.txt" "$DIR/$CASE.c" 2>/dev/null || true
  [ -s "${OUTBIN%.$DYLIBEXT}.libs.txt" ] && echo "[c99native] libs.txt -> ${OUTBIN%.$DYLIBEXT}.libs.txt ($(grep -cv '^#' "${OUTBIN%.dylib}.libs.txt" | tr -d ' ') lib)" >&2
fi

# Esegue (CWD = cartella del sample) per un eseguibile; per una libreria si verifica solo
# che i simboli esportati siano presenti.
# CROSS (F5): un binario per un altro RID si produce e si verifica, NON si esegue — chiedere al
# loader dell'host un ELF eseguibile su macOS produce solo un errore di exec fuorviante.
# `HOST_RID` è calcolato in cima (serve alla scelta del runtime). Qui si stampa anche COME
# eseguirlo, così un caso cross si prova senza passare dalla suite.
if grep -q "int main" "$DIR/$CASE.c" && [ "$RID" != "$HOST_RID" ]; then
  echo "[c99native] cross $RID: binario prodotto ($OUTBIN), non eseguito su $HOST_RID" >&2
  case "$RID" in
    linux-x64|linux-arm64)
      _plat="linux/amd64"; [ "$RID" = linux-arm64 ] && _plat="linux/arm64"
      echo "  esecuzione (Docker, usa solo la cache del RID):" >&2
      echo "    docker run --rm --platform $_plat -v \"$OUTBIN:/p\" -v \"$LIBS_CACHE:$LIBS_CACHE:ro\" \\" >&2
      echo "      -e LD_LIBRARY_PATH=$LIBS_CACHE ubuntu:24.04 /p" >&2
      ;;
    win-*)
      echo "  esecuzione: serve Windows/Wine; il PE32+ è in $OUTBIN" >&2
      ;;
  esac
elif grep -q "int main" "$DIR/$CASE.c"; then
  GMB_TORCH_PATH="${GMB_TORCH_PATH:-$HOME/.gmbasic/libs/ml/$RID}"
  (cd "$DIR" && GMB_TORCH_PATH="$GMB_TORCH_PATH" ./bin/"$(basename "$CASE")" ${PROG_ARGS[@]+"${PROG_ARGS[@]}"})
else
  # Verifica gli export della libreria (evita pipe con head: con `set -o pipefail` un lettore che
  # chiude presto manda SIGPIPE e farebbe fallire lo script).
  nm $NM_GU "$OUTBIN" > "$DIR/$CASE.syms" 2>/dev/null || true
  LIBPREFIX=$(sed -nE 's/.*static void ([A-Za-z0-9_]+)__ctor.*/\1/p' "$DIR/$CASE.c" | sed -n '1p')
  if [ -n "$LIBPREFIX" ] && grep -q " T _${LIBPREFIX}_init" "$DIR/$CASE.syms"; then
    NE=$(grep -c " T _${LIBPREFIX}_" "$DIR/$CASE.syms" || true)
    echo "LIB ${LIBPREFIX}.dylib export OK (${NE} simboli)" >&2
  else
    echo "LIB: dylib costruita ma export non trovati (prefix='${LIBPREFIX}')" >&2
  fi
fi
