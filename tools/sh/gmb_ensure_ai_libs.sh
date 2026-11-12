#!/bin/zsh
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

# gmb_ensure_ai_libs.sh — GARANZIA anti-stale GLOBALE: ricostruisce SEMPRE le lib/dylib AI native
# (incrementale, idempotente). Invocato PRIMA della build di OGNI consumer nativo (modulo CMake
# `native/cmake/GmbAiNativeLibs.cmake`) e dai consumer C# (target MSBuild). Cosi' nessuna build
# puo' usare una lib AI VECCHIA: o si aggiorna automaticamente, o (se il build dir manca) la
# build si blocca con errore esplicito.
#
# Le sorgenti AI: gmbllm/ (PUL), native/gmb_runtime_c/, LLMEnv/LLMCodec (Apache).
set -e
ROOT="$(cd "$(dirname "$0")/../../GMBasic" && pwd)"
# I TOOL vivono nella radice del MONOREPO (`tools/`), non più in GMBasic/tools.
MONO="$(cd "$(dirname "$0")/../.." && pwd)"

RT="$ROOT/native/gmb_runtime_c/build"
CC="$ROOT/native/gmb_compiler_c/build"

if [ ! -d "$RT" ]; then
  echo "ERRORE: build dir assente ($RT). Configura prima: cmake -S native/gmb_runtime_c -B $RT" >&2
  exit 1
fi

# 1) lib statiche AI (libgmb_runtime.a, libgmbllm_core.a, libgmbllm_train.a) + dylib gmb_ai_llm/gmb_runtime_ai.
#    NIENTE `|| true`: se il build AI fallisce la build del consumer DEVE fallire (mai stale silenzioso).
  cmake --build "$RT" --target gmb_runtime gmb_runtime_ai gmbllm_core gmbllm_train gmb_ai_llm -j 6 >/dev/null
  # 1a) EXPORT MANIFEST: le dylib appena ricostruite possono avere IMPORT NUOVI, e le nostre dylib
  #     li risolvono a load-time nell'eseguibile. Il file che il runner legge al link
  #     (`build/libs/<rid>/libgmb.exports.txt`) va quindi riscritto QUI: se restasse quello di un
  #     bundle precedente, un import nuovo resterebbe scoperto e l'eseguibile avrebbe uno stub NULL
  #     (crash in un inizializzatore, prima di `main`). Non blocca il build se dyld_info manca.
  if [ -f "$MONO/tools/sh/libs_exports_file.sh" ] && command -v dyld_info >/dev/null 2>&1; then
    bash "$MONO/tools/sh/libs_exports_file.sh" write "$ROOT/build/libs/osx-arm64" || \
      echo "gmb_ensure_ai_libs: ATTENZIONE libgmb.exports.txt non rigenerato" >&2
  fi

# 1b) `gmb_matrix_flat` (NON AI): ABI piatta dei kernel matriciali per il P/Invoke dei backend
#     managed (IL/PCode → stessa sorgente di VM/C e C99Native). Comando SEPARATO: su un albero
#     di build non ancora riconfigurato il target non esiste e `make` fallirebbe in fase di
#     parsing; se manca, si riconfigura una volta (riusa la cache).
if ! cmake --build "$RT" --target gmb_matrix_flat -j 6 >/dev/null 2>&1; then
  cmake -S "$ROOT/native/gmb_runtime_c" -B "$RT" >/dev/null
  cmake --build "$RT" --target gmb_matrix_flat -j 6 >/dev/null
fi
# 2) dylib gmb_ai_modelio (model compiler, P/Invoke) — albero separato (gmb_compiler_c).
#    Se il build fallisce -> ERRORE (prima c'era `|| true` che lasciava una dylib STALE).
if [ -d "$CC" ]; then
  cmake --build "$CC" --target gmb_ai_modelio -j 6 >/dev/null
fi
# 2b) GARANZIA FRESCHEZZA (NON DEROGABILE): la dylib deployata che espone il MODEL COMPILER
#     (`libgmb_ai_llm`, che linka gmbllm) DEVE essere piu' recente dei sorgenti che finiscono
#     DAVVERO dentro di essa. Se e' STALE -> build BLOCCATA (meglio non compilare che usare
#     codice vecchio: era la causa di una divergenza IL vs C99Native sul model compiler).
#
#     ATTENZIONE (fix 2026-10-28): il confronto NON puo' usare TUTTO `native/gmb_runtime_c/src`.
#     Quella cartella contiene anche i sorgenti di ALTRI target (`gmb_pkg` → gmb_pkg_*.c,
#     `gmb_sql_ext`, …) che non entrano in `libgmb_runtime.a` e quindi nemmeno nella dylib.
#     Con il confronto largo, modificare `gmb_pkg_resolver.c` marcava STALE una dylib in realta'
#     aggiornata e BLOCCAVA ogni build nativa (caso reale: build di `gmb_vm_host` ferma per ore).
#     Criterio corretto: i sorgenti i cui oggetti sono DENTRO l'archivio linkato dalla dylib
#     (`ar t libgmb_runtime.a`), piu' i sorgenti gmbllm (compilati direttamente nella dylib).
RT_LIB="$RT/libgmb_runtime.a"
NEWEST_SRC=""
if [ -e "$RT_LIB" ]; then
  RT_OBJS="$(ar t "$RT_LIB" 2>/dev/null | sed 's/\.c\.o$//;s/\.o$//' | sort -u)"
  for src in $(find "$ROOT/native/gmb_runtime_c/src" -name '*.c' 2>/dev/null); do
    if printf '%s\n' "$RT_OBJS" | grep -qx "$(basename "$src" .c)"; then
      if [ -z "$NEWEST_SRC" ] || [ "$src" -nt "$NEWEST_SRC" ]; then NEWEST_SRC="$src"; fi
    fi
  done
fi
for src in $(find "$ROOT/gmbllm/src" -name '*.c' 2>/dev/null); do
  if [ -z "$NEWEST_SRC" ] || [ "$src" -nt "$NEWEST_SRC" ]; then NEWEST_SRC="$src"; fi
done
AILLM_LIB="$ROOT/build/libs/osx-arm64/libgmb_ai_llm.dylib"
if [ -e "$AILLM_LIB" ] && [ -n "$NEWEST_SRC" ] && [ "$NEWEST_SRC" -nt "$AILLM_LIB" ]; then
  echo "gmb_ensure_ai_libs: ERRORE — $AILLM_LIB e' STALE rispetto a $NEWEST_SRC" >&2
  echo "  La dylib deployata non e' stata ricostruita dal target CMake: build BLOCCATA." >&2
  exit 1
fi
# 2c) LLMCodec (UNIFICAZIONE BUILD): ogni albero CMake che fa `add_subdirectory(LLMCodec)` produce
#     la PROPRIA copia di libllmcodec.a dalla STESSA sorgente (LLMEnv/LLMCodec/src). Se una copia e'
#     piu' vecchia di un sorgente/header LLMCodec, il consumer userebbe una lib STALE con LAYOUT
#     VECCHIO (skew di struct -> crash, es. rc=138). Qui, per OGNI copia nota: se stale -> ricompila
#     il suo target; se resta stale -> STOP (mai compilare con codice non allineato).
NEWEST_CODEC=$(find "$LLMENV/LLMCodec/src" "$LLMENV/LLMCodec/include" -name '*.c' -o -name '*.h' 2>/dev/null | xargs ls -t 2>/dev/null | head -1)
if [ -n "$NEWEST_CODEC" ]; then
  # "builddir:target" per ogni albero che costruisce LLMCodec
  for pair in \
      "$LLMENV/build:llmcodec" \
      "$LLMENV/LLMEngine/mlx/build-gmbasic:llmcodec" \
      "$ROOT/native/gmb_runtime_c/build:n" \
      "$ROOT/native/gmb_vm_c/build:n" \
      "$ROOT/native/gmb_compiler_c/build:n" ; do
    bd="${pair%%:*}"; tg="${pair##*:}"
    [ -f "$bd/CMakeCache.txt" ] || continue
    # lib piu' recente prodotta in quell'albero
    lib=$(find "$bd" -name 'libllmcodec.a' -newer "$NEWEST_CODEC" 2>/dev/null | head -1)
    if [ -z "$lib" ]; then
      cmake --build "$bd" --target "$tg" -j 6 >/dev/null 2>&1 || true
      lib=$(find "$bd" -name 'libllmcodec.a' -newer "$NEWEST_CODEC" 2>/dev/null | head -1)
      if [ -z "$lib" ]; then
        echo "gmb_ensure_ai_libs: ERRORE — llmcodec STALE in $bd (target $tg) vs $NEWEST_CODEC" >&2
        exit 1
      fi
    fi
  done
fi
# 2d) MOTORE MLX (`libgmb_ai_mlx.dylib`): la dylib DEPLOYATA deve essere piu' recente dei sorgenti
#     del motore (LLMEngine/mlx + LLMMetal + LLMCodec). Se STALE -> ricostruisci build-gmbasic e
#     re-installa (atomico via install_mlx_dylib.sh). Cosi' GMBasic non usa MAI un engine MLX vecchio
#     (era l'unico artefatto AI fuori dal controllo anti-stale). Se resta stale -> build BLOCCATA.
MLXENV="${LLMENV:-$ROOT/../LLMEnv}"
MLX_BUILD="$MLXENV/LLMEngine/mlx/build-gmbasic"
MLX_SRC="$MLX_BUILD/libllm_ai_mlx.dylib"
MLX_DEPLOYED="$ROOT/build/libs/osx-arm64/libgmb_ai_mlx.dylib"
# `metal/` del vendored ds4 (usata a runtime dal motore DS4 vendorizzato): piccola (file sorgente
# .metal), deployata accanto alla dylib. Senza, il vendored non parte e scatta il fallback ds4-slow.
if [ -d "$MLXENV/LLMEngine/ds4engine/metal" ]; then
  mkdir -p "$ROOT/build/libs/osx-arm64/metal"
  cp -R "$MLXENV/LLMEngine/ds4engine/metal/." "$ROOT/build/libs/osx-arm64/metal/"
fi
# La dylib deployata deve esporre l'API COMPLETA del motore: il mtime da solo NON basta (una
# variante errata — es. `gmb_ai_mlx_*` priva di session_logprob_of — piu' NUOVA dei sorgenti
# restava deployata -> PPL/QUALITY impossibili). Accetta entrambi i prefissi.
mlx_symbols_ok() {
  local d="$1" syms s
  syms=$(nm -gU "$d" 2>/dev/null || nm -g "$d" 2>/dev/null || true)
  for s in load_model create_session session_append session_logprob_of; do
    printf '%s\n' "$syms" | grep -qE "_(llm|gmb)_ai_mlx_${s}\$" || return 1
  done
  return 0
}
if [ -d "$MLX_BUILD" ] && [ -f "$MLX_DEPLOYED" ]; then
  NEWEST_MLX=$( { find "$MLXENV/LLMEngine/mlx" "$MLXENV/LLMMetal" "$MLXENV/LLMCodec/src" "$MLXENV/LLMCodec/include" \
                    -path '*/ds4-slow/*' -prune -o \
                    \( -name '*.c' -o -name '*.h' -o -name '*.mm' -o -name '*.cpp' \) -print 2>/dev/null; } \
               | xargs ls -t 2>/dev/null | head -1 )
  MLX_REASON=""
  if [ -n "$NEWEST_MLX" ] && [ "$NEWEST_MLX" -nt "$MLX_DEPLOYED" ]; then MLX_REASON="sorgenti piu' nuovi";
  elif ! mlx_symbols_ok "$MLX_DEPLOYED"; then MLX_REASON="simboli engine incompleti (session_logprob_of?)"; fi
  if [ -n "$MLX_REASON" ]; then
    if ! cmake --build "$MLX_BUILD" --target llm_ai_mlx -j 6 >/dev/null 2>&1; then
      echo "gmb_ensure_ai_libs: ERRORE — build del motore MLX FALLITA ($MLX_BUILD)" >&2
      exit 1
    fi
    if [ -f "$MLX_SRC" ]; then
      /bin/bash "$MONO/tools/sh/install_mlx_dylib.sh" "$MLX_SRC" "$MLX_DEPLOYED" >/dev/null 2>&1 || true
    fi
    if [ ! -f "$MLX_DEPLOYED" ]; then
      echo "gmb_ensure_ai_libs: ERRORE — libgmb_ai_mlx assente ($MLX_DEPLOYED)" >&2
      exit 1
    fi
    # Dopo il reinstall DEVE essere completa: se resta incompleta, blocca (meglio non compilare
    # che usare un engine senza session_logprob_of). Ora il gate e' sui SIMBOLI, non sul mtime.
    if ! mlx_symbols_ok "$MLX_DEPLOYED"; then
      echo "gmb_ensure_ai_libs: ERRORE — libgmb_ai_mlx deployata senza l'API completa del motore ($MLX_DEPLOYED)" >&2
      exit 1
    fi
  fi
fi
# 3) TOKENIZER C (LLMEnv): i runner C99 (gate) e C99Native linkano
#    $LLMEnv/build/dist/osx-arm64/libllmtoken.dylib. Il target `llmtoken` lo produce in
#    build/LLMToken/ -> si copia nel path atteso (idempotente). Senza il dylib il link dei
#    sample LLM fallisce con "Undefined _llmtoken_*" (gate rosso falso).
LLMENV="${LLMENV:-$ROOT/../LLMEnv}"
TOKDST="$LLMENV/build/dist/osx-arm64"
TOKLIB="$LLMENV/build/LLMToken/libllmtoken.dylib"
if [ ! -f "$TOKLIB" ] && [ -d "$LLMENV/build" ]; then
  cmake --build "$LLMENV/build" --target llmtoken -j 6 >/dev/null 2>&1 || true
fi
if [ -f "$TOKLIB" ] && { [ ! -f "$TOKDST/libllmtoken.dylib" ] || [ "$TOKLIB" -nt "$TOKDST/libllmtoken.dylib" ]; }; then
  mkdir -p "$TOKDST" && cp "$TOKLIB" "$TOKDST/libllmtoken.dylib"
fi
# 3b) PACKAGE MANAGER (gmb_pkg) — riferimento C del PM, P/Invoke dal C# (§0bis C-first).
#     La dylib deployata deve essere aggiornata: senza, il resolver C# (NativePkg) non la carica.
GMB_PKG_BUILD="$ROOT/native/gmb_runtime_c/build"
if [ -d "$GMB_PKG_BUILD" ]; then
  cmake --build "$GMB_PKG_BUILD" --target gmb_pkg -j 6 >/dev/null 2>&1 || true
  if [ -f "$GMB_PKG_BUILD/libgmb_pkg.dylib" ]; then
    DST="$ROOT/build/libs/osx-arm64/libgmb_pkg.dylib"
    mkdir -p "$(dirname "$DST")"
    { [ ! -f "$DST" ] || [ "$GMB_PKG_BUILD/libgmb_pkg.dylib" -nt "$DST" ]; } && cp "$GMB_PKG_BUILD/libgmb_pkg.dylib" "$DST"
  elif [ -f "$GMB_PKG_BUILD/libgmb_pkg.so" ]; then
    DST="$ROOT/build/libs/linux-x64/libgmb_pkg.so"
    mkdir -p "$(dirname "$DST")"
    { [ ! -f "$DST" ] || [ "$GMB_PKG_BUILD/libgmb_pkg.so" -nt "$DST" ]; } && cp "$GMB_PKG_BUILD/libgmb_pkg.so" "$DST"
  fi
fi
# 3bis) ESTENSIONI SQL (`gmb_sql_csv_ext` / `gmb_sql_parquet_ext`): qui il CANONICO è il BUILD
#       (`native/gmb_sql_ext/build`), non la cartella degli artefatti (`build/libs/<rid>`) — il loro
#       CMake non ha output dir e `SqlCsvExtension.ResolvePath` cerca prima la cache. Senza questo
#       passo la copia installata può restare indietro in silenzio (stato trovato il 2026-10-27:
#       build dir del 28/09, copie del 26/10 con sorgenti già diversi).
#       Il build è INCREMENTALE: se nessun sorgente è cambiato non tocca l'output, quindi non c'è
#       churn. Confronto per MTIME (non per contenuto): due build degli stessi sorgenti non sono
#       byte-identiche. Va PRIMA del passo 4, così le copie installate vengono firmate ad-hoc.
SQL_EXT_BUILD="$ROOT/native/gmb_sql_ext/build"
if [ -f "$SQL_EXT_BUILD/CMakeCache.txt" ]; then
  cmake --build "$SQL_EXT_BUILD" --target gmb_sql_csv_ext gmb_sql_parquet_ext -j 6 >/dev/null 2>&1 || {
    echo "gmb_ensure_ai_libs: ERRORE — build di gmb_sql_*_ext fallita ($SQL_EXT_BUILD)" >&2
    exit 1
  }
  for _n in gmb_sql_csv_ext gmb_sql_parquet_ext; do
    _src="$SQL_EXT_BUILD/$_n.dylib"
    [ -f "$_src" ] || continue
    for _dst in "$ROOT/build/libs/osx-arm64/$_n.dylib"; do
      [ -d "$(dirname "$_dst")" ] || continue
      if [ ! -f "$_dst" ] || [ "$_src" -nt "$_dst" ]; then
        # Atomico: il modulo puo' essere mappato in un processo vivo (SqliteConnection.LoadExtension).
        bash "$MONO/tools/sh/install_mlx_dylib.sh" "$_src" "$_dst" >/dev/null 2>&1 || true
      fi
      # GARANZIA: la copia deployata non puo' restare piu' vecchia del build.
      if [ ! -f "$_dst" ] || [ "$_src" -nt "$_dst" ]; then
        echo "gmb_ensure_ai_libs: ERRORE — estensione SQL deployata non aggiornata: $_dst (build: $_src)" >&2
        exit 1
      fi
    done
  done
fi

# 4) FIRMA AD-HOC delle dylib DEPLOYATE. Su Apple Silicon una dylib *linker-signed* (firma ad-hoc
#    emessa dal linker) fa SIGKILLARE con "Code Signature Invalid / Invalid Page" QUALUNQUE processo
#    che la carica (gate C99, test_ilrun/test_vmrun, REPL) — e `codesign -v` la vede "valid on disk",
#    quindi il problema NON è rilevabile così. Si ri-firmano ad-hoc SOLO le linker-signed
#    (idempotente; non tocca quelle già firmate con `codesign`). Diagnosi: crash report macOS
#    `termination.namespace=CODESIGNING`, `signal=SIGKILL (Code Signature Invalid)`.
if [ "$(uname -s)" = "Darwin" ] && command -v codesign >/dev/null 2>&1; then
  for dir in "$ROOT/build/libs/osx-arm64"; do
    # NB: niente glob `"$dir"/*.dylib` — sotto `set -e` un glob senza match ABORTISCE lo script
    # (zsh: "no matches found"; e `(N)` è solo-zsh, mentre il file viene invocato anche con bash).
    # `find` è portabile e non fallisce se la directory non esiste (caso reale 2026-10-28:
    # la cartella può non esistere: `find` non fallisce).
    find "$dir" -maxdepth 1 -name '*.dylib' 2>/dev/null | while IFS= read -r d; do
      [ -f "$d" ] || continue
      if codesign -dvv "$d" 2>&1 | grep -q 'linker-signed'; then
        codesign --force --sign - "$d" 2>/dev/null || true
      fi
    done
  done
fi

# 5) NIENTE PIÙ MIRROR. Prima le dylib nostre si copiavano anche in `Resources/libs/<rid>/`
#    perché alcuni consumer le aprivano per PERCORSO ASSOLUTO: era una seconda copia da tenere
#    allineata a mano (e una seconda istanza rompe il single-instance MLX). Ora esiste UNA sola
#    cartella di artefatti nostri — `build/libs/<rid>/` — che sta nell'rpath e nel
#    `DYLD_LIBRARY_PATH` dei runner; in distribuzione la stessa dylib arriva dalla cache
#    (`~/.gmbasic/libs/<rid>/`), quindi non serve nessuna copia di deploy.
echo "gmb_ensure_ai_libs: lib/dylib AI native aggiornate ($(basename "$ROOT"))"
