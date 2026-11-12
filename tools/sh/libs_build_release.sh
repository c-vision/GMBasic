#!/usr/bin/env bash
# libs_build_release.sh — COMPILA, RACCOGLIE, VERIFICA e (su richiesta) PUBBLICA un gruppo di
# librerie native per il RID della macchina corrente (o per --rid), poi aggiorna l'indice.
#
# PERCHÉ ESISTE. I bundle per gli altri RID non esistevano perché la pubblicazione partiva da
# artefatti già presenti (`build/libs/<rid>` per i nostri, cache utente per le terze parti), cioè da
# ciò che era compilato a mano sulla macchina di turno. Questo script rende il passo RIPETIBILE
# su qualunque OS: compila i progetti dichiarati, raccoglie i file con le regole di
# `libs_groups.sh`, VERIFICA che il set copra quello pubblicato (nessun buco silenzioso), passa
# il gate di caricabilità (`libs_verify_load.sh`) e solo allora pubblica.
#
# USO
#   tools/sh/libs_build_release.sh <gruppo> [opzioni]
#     gruppo: core | engine | llmmotor | support
#     --rid RID          default: RID della macchina (stessa mappatura di GmbRid.cmake/resolver)
#     --no-build         non compila: raccoglie ciò che è già compilato (staging+verifica)
#     --publish          pubblica su c-vision/GMBasicModules (release + indice)
#     --tag TAG          tag della release (default: gmb-<gruppo>-<data>)
#     --allow-gaps       pubblica anche se il set non copre l'indice (SCONSIGLIATO, lo stampa)
#     --no-index         non rigenera l'indice (per CI: l'indice si aggiorna in un job unico,
#                        altrimenti N RID in parallelo si sovrascrivono lo stesso manifest)
#     --stage-dir DIR    dove lasciare i file raccolti (default: temp, cancellata all'uscita).
#                        Serve al runner in container: il bundle esce su un volume montato.
#     --deps DIR         (gruppo `dev`) terze parti del RID: attributi `include/` e `lib/`
#                        (default: ~/gmb-deps/<triple>, dove le mette tools/c/build-deps.sh).
#     --build-root DIR   albero di build separato (default: <progetto>/build). Serve quando il
#                        checkout è montato da un ALTRO OS (container): il CMakeCache del host
#                        (macOS) non è riutilizzabile in Linux → si configura in DIR/<progetto>.
#     --cmake-arg "X"    argomento extra per OGNI configure (ripetibile). Serve alla build CROSS,
#                        es: --cmake-arg "-DCMAKE_OSX_ARCHITECTURES=x86_64" --cmake-arg "-DGMB_RID=osx-x64"
#     --no-verify        salta il gate dlopen (SCONSIGLIATO)
#     --dry-run          stampa il piano (compilazioni, staging, gate) e non pubblica
#     --yes              non chiede conferma
#     -h|--help
#
# GRUPPI
#   core       librerie NOSTRE di GMBasic (runtime C, gfx, screen, pkg, compress, grep, tray,
#              ai_llm, ai_modelio, sql ext, torch wrapper): compilate QUI.
#   engine     plumbing AI (libllmcodec_*, libllmtoken, …): prodotte da LLMEnv →
#              `$LLMENV/tools/publish_libs_gmbasicmodules.sh` (incluso FastLLM/MLX).
#   llmmotor   motori AI (MLX + llama.cpp): come `engine`. Su Linux/Windows il motore è SOLO
#              llama.cpp: MLX (`libllm_ai_mlx`, `mlx.metallib`) è Apple-only per costruzione.
#   support    terze parti: bundle curato, NON si compila qui (vedi libs_publish_gh.sh support).
#
# ESEMPI
#   tools/sh/libs_build_release.sh core --dry-run
#   tools/sh/libs_build_release.sh core --no-build            # staging+verifica senza compilare
#   tools/sh/libs_build_release.sh core --publish --yes       # build + gate + upload + indice
#   tools/sh/libs_build_release.sh engine --publish --yes     # delega a LLMEnv
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../GMBasic" && pwd)"
. "$SCRIPT_DIR/libs_groups.sh"

GROUP="${1:-}"; [ $# -gt 0 ] && shift
[ -n "$GROUP" ] || { sed -n '2,45p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }
# Il gruppo `ffi` (nostre lib di gmbsuite: amqp, grpc, kafka, mqtt, nats, protobuf, media) NON si
# costruisce qui: i sorgenti vivono in `gmbsuite/` e pubblicano i loro `build.sh --publish`
# (helper `gmbsuite/tools/gmb_lib.sh` → `libs_publish_files_gh.sh ffi <rid>`). Qui si indicizza
# soltanto (`libs_manifest_gh.sh build`), che tratta `ffi` come gli altri gruppi.

RID=""; DO_BUILD=1; PUBLISH=0; TAG=""; ALLOW_GAPS=0; DO_VERIFY=1; DRY=0; YES=0; NO_INDEX=0
STAGE_DIR=""; BUILD_ROOT=""; DEPS_DIR=""; CMAKE_ARGS=()
while [ $# -gt 0 ]; do
  case "$1" in
    --rid) RID="${2:-}"; shift 2;;
    --no-build) DO_BUILD=0; shift;;
    --publish) PUBLISH=1; shift;;
    --tag) TAG="${2:-}"; shift 2;;
    --allow-gaps) ALLOW_GAPS=1; shift;;
    --no-index) NO_INDEX=1; shift;;
    --stage-dir) STAGE_DIR="${2:-}"; shift 2;;
    --build-root) BUILD_ROOT="${2:-}"; shift 2;;
    --deps)       DEPS_DIR="${2:-}"; shift 2;;
    --cmake-arg) CMAKE_ARGS+=("${2:-}"); shift 2;;
    --no-verify) DO_VERIFY=0; shift;;
    --dry-run) DRY=1; shift;;
    --yes) YES=1; shift;;
    -h|--help) sed -n '2,45p' "$0" | sed 's/^# \{0,1\}//'; exit 0;;
    *) echo "opzione sconosciuta: $1" >&2; exit 2;;
  esac
done
case "$GROUP" in runtime|runtime-core|boot|dev|core|engine|llmmotor|support|toolchain) ;; *) echo "gruppo non valido: $GROUP (runtime|runtime-core|boot|dev|core|engine|llmmotor|support|toolchain)" >&2; exit 2 ;; esac

# ── RID (stessa mappatura di native/cmake/GmbRid.cmake e LibsResolver.Rid) ───────────────────
if [ -z "$RID" ]; then
  case "$(uname -s)-$(uname -m)" in
    Darwin-arm64)          RID=osx-arm64;;
    Darwin-x86_64)         RID=osx-x64;;
    Linux-x86_64|Linux-amd64) RID=linux-x64;;
    Linux-aarch64|Linux-arm64) RID=linux-arm64;;
    MINGW*|MSYS*|CYGWIN*)  case "$(uname -m)" in aarch64|arm64) RID=win-arm64;; *) RID=win-x64;; esac;;
    *) echo "RID non riconosciuto ($(uname -s)-$(uname -m)): passare --rid" >&2; exit 2;;
  esac
fi
# ── TOOLCHAIN (F1): il COMPILATORE, non una libreria ───────────────────────────────────────
# Un artefatto per host, senza compilare nulla: delega a `libs_toolchain_pack.sh` (che è anche
# l'unico punto che genera `THIRD_PARTY_NOTICES` e la riga d'indice). La PUBBLICAZIONE resta al
# canale unico degli asset, quindi `--publish` qui NON si applica: si stampa il comando esatto
# (D13: un solo writer dell'indice, il job `index`).
if [ "$GROUP" = "toolchain" ]; then
  # La VERSIONE di zig ha un pin unico: dentro `libs_toolchain_pack.sh` (qui non si duplica).
  args=(--rid "$RID")
  [ "$DRY" = "1" ] && args+=(--dry-run)
  [ -n "$STAGE_DIR" ] && args+=(--out "$STAGE_DIR")
  if [ "$PUBLISH" = "1" ]; then
    echo "NOTA: --publish non si applica a 'toolchain': usare libs_publish_files_gh.sh." >&2
  fi
  echo "-> delega a: libs_toolchain_pack.sh ${args[*]}"
  exec bash "$SCRIPT_DIR/libs_toolchain_pack.sh" "${args[@]}"
fi

IS_APPLE=0; [ "$(uname -s)" = "Darwin" ] && IS_APPLE=1
# LLMEnv (motori/codec AI): env LLMENV, poi i layout noti (monorepo, Projects, HOME).
if [ -z "${LLMENV:-}" ]; then
  for c in "$ROOT/../LLMEnv" "$ROOT/../../LLMEnv" "$HOME/Documents/Projects/LLMEnv"; do
    [ -f "$c/tools/publish_libs_gmbasicmodules.sh" ] && { LLMENV="$c"; break; }
  done
  LLMENV="${LLMENV:-$ROOT/../LLMEnv}"
fi
MANIFEST="${MANIFEST:-$ROOT/../shared/settings/libs-manifest.json}"

LIBS_RID="$RID"   # le regole di gruppo guardano il RID di DESTINAZIONE (non l'host)
echo "== libs_build_release: gruppo=$GROUP rid=$RID (build=$DO_BUILD publish=$PUBLISH dry=$DRY)"

# ── engine / llmmotor: le produce LLMEnv (non GMBasic) ───────────────────────────────────────
# Due strade:
#  a) LOCALE (preferita quando c'è un build dir di gmb_runtime_c): i file di `engine` sono già
#     compilati COME SOTTOPROGETTI LLMEnv dentro quel build (llmcodec_build/, llmtoken_build/, …),
#     quindi si compilano con lo stesso `cmake --build` del gruppo core e si raccolgono da lì.
#     È ciò che serve in container (layout Linux) e in CI, dove il publisher di LLMEnv (che assume
#     il layout macOS) non è utilizzabile.
#  b) DELEGA a `LLMEnv/tools/publish_libs_gmbasicmodules.sh` quando la (a) non è disponibile.
ENGINE_LOCAL=0
if { [ "$GROUP" = "engine" ] || [ "$GROUP" = "llmmotor" ]; } && [ -n "$BUILD_ROOT" ]; then
  ENGINE_LOCAL=1
fi

if { [ "$GROUP" = "engine" ] || [ "$GROUP" = "llmmotor" ]; } && [ "$ENGINE_LOCAL" = "0" ]; then
  PUB="$LLMENV/tools/publish_libs_gmbasicmodules.sh"
  if [ ! -f "$PUB" ]; then
    echo "ERRORE: $PUB non trovato." >&2
    echo "  engine/llmmotor sono prodotte da LLMEnv, non da GMBasic: serve il checkout LLMEnv" >&2
    echo "  (env LLMENV, default $ROOT/../LLMEnv)." >&2
    echo "  In alternativa: --build-root <dir> per compilarle dai sottoprogetti (container/CI)." >&2
    exit 1
  fi
  args=("$RID")
  [ "$DO_BUILD" = "1" ] || args+=(--skip-build)
  if [ "$PUBLISH" = "1" ] && [ "$NO_INDEX" = "0" ]; then args+=(--index --push-manifest); fi
  [ "$YES" = "1" ] && args+=(--yes)
  [ "$DRY" = "1" ] && args+=(--dry-run)
  if [ "$GROUP" = "llmmotor" ] && [ "$IS_APPLE" = "0" ]; then
    echo "NOTA: su $RID il motore MLX non esiste (Metal è Apple-only): il gruppo conterrà solo"
    echo "      llama.cpp (libllm_ai_llamacpp + wrapper). È il comportamento atteso."
  fi
  echo "-> delega a: $PUB ${args[*]}"
  [ "$DRY" = "1" ] && { echo "(dry-run: non eseguo)"; exit 0; }
  exec bash "$PUB" "${args[@]}"
fi

if [ "$GROUP" = "support" ]; then
  echo "ERRORE: il gruppo 'support' è terze parti (bundle curato), non si compila qui." >&2
  echo "  Pubblicazione: tools/sh/libs_publish_gh.sh support <rid> --source <cartella curata>" >&2
  exit 2
fi

# ── CORE: compilazione dei progetti dichiarati ───────────────────────────────────────────────
# Ogni voce: <progetto> <build dir> <target...> — i target SHARED scrivono in build/libs/<rid>
# (LIBRARY_OUTPUT_DIRECTORY nel CMake) oppure nella loro build dir (compress/grep, sql ext).
build_core() {
  local -a plan
  # gmb_runtime_c: gfx (SDL), screen (buffer testo), pkg, compress, grep, tray, ai_llm
  plan=(
    "$ROOT/native/gmb_runtime_c|gmb_gfx gmb_screen gmb_pkg gmb_compress gmb_grep gmbclm gmb_tray gmb_ai_llm"
    "$ROOT/native/gmb_compiler_c|gmb_ai_modelio"
    "$ROOT/native/gmb_sql_ext|gmb_sql_csv_ext gmb_sql_parquet_ext"
  )
  local entry proj targets bdir
  for entry in "${plan[@]}"; do
    proj="${entry%%|*}"; targets="${entry##*|}"
    bdir="$(build_dir "$proj")"
    if [ ! -d "$bdir" ]; then
      if [ -n "$BUILD_ROOT" ]; then
        echo "-> configure $proj -> $bdir"
        [ "$DRY" = "1" ] || cmake -S "$proj" -B "$bdir" -DCMAKE_BUILD_TYPE=Release ${CMAKE_ARGS[@]+"${CMAKE_ARGS[@]}"} || return 1
      else
        echo "ERRORE: build dir assente: $bdir" >&2
        echo "  configurala: cmake -S \"$proj\" -B \"$bdir\"" >&2
        return 1
      fi
    fi
    echo "-> build $proj: $targets  ($bdir)"
    [ "$DRY" = "1" ] && continue
    # shellcheck disable=SC2086
    cmake --build "$bdir" --target $targets -j 6 || return 1
  done
  # torch wrapper: opzionale (dipende dal pacchetto torch, gruppo `external`)
  if [ -d "$ROOT/native/gmb_torch/build" ]; then
    echo "-> build $ROOT/native/gmb_torch (opzionale: richiede torch)"
    [ "$DRY" = "1" ] || cmake --build "$ROOT/native/gmb_torch/build" -j 6 \
      || echo "   (torch NON compilato: il wrapper libgmb_torch* resterà fuori — gruppo external)"
  fi
  return 0
}

# build dir di un progetto: <progetto>/build, oppure <BUILD_ROOT>/<progetto> se richiesto
# (container: il CMakeCache del host non è riutilizzabile su un altro OS).
build_dir() {
  if [ -n "$BUILD_ROOT" ]; then echo "$BUILD_ROOT/$(basename "$1")"; else echo "$1/build"; fi
}

build_engine_local() {
  local bdir; bdir="$(build_dir "$ROOT/native/gmb_runtime_c")"
  if [ ! -d "$bdir" ]; then
    echo "-> configure gmb_runtime_c -> $bdir"
    [ "$DRY" = "1" ] || cmake -S "$ROOT/native/gmb_runtime_c" -B "$bdir" -DCMAKE_BUILD_TYPE=Release ${CMAKE_ARGS[@]+"${CMAKE_ARGS[@]}"} || return 1
  fi
  echo "-> build gmb_runtime_c (include i sottoprogetti LLMEnv: codec/token/desc/moe)"
  # Il `;` dopo l'echo MANCAVA: `echo ... [ "$DRY" = "1" ] || cmake …` passava il test come
  # ARGOMENTO dell'echo, l'echo usciva 0 e la build non veniva mai eseguita (gruppi con
  # ENGINE_LOCAL: engine/llmmotor). Ora si esegue davvero e l'errore si propaga.
  [ "$DRY" = "1" ] || cmake --build "$bdir" -j 6 || return 1
}

# ── GRUPPO `dev` (F3 del piano Toolchain-Zig) ────────────────────────────────────────────────
# Il payload per COMPILARE non sono librerie dinamiche: sono `include/**` (terze parti + header
# nostri + gli header generati nel build, es. `gmb_gfx_spirv.h`) e `lib/*.a` (statiche nostre +
# archivi delle terze parti). Lo staging generico qui sotto cerca solo `.dylib/.so/.dll` e con la
# regola `dev` raccoglierebbe quasi nulla: per questo il gruppo ha il suo staging dedicato, e il
# gate `libs_dev_selfcheck.sh` misura ciò che conta (gli include e le `-l` risolti NEL bundle).
rid_triple() {
  # $1 = RID → triple (tabella §17.1 del piano: RID ≠ triple)
  case "$1" in
    osx-arm64)   echo aarch64-macos;;
    osx-x64)     echo x86_64-macos;;
    linux-x64)   echo x86_64-linux-gnu;;
    linux-arm64) echo aarch64-linux-gnu;;
    win-x64)     echo x86_64-windows-gnu;;
    win-arm64)   echo aarch64-windows-gnu;;
    *) return 1;;
  esac
}

stage_dev() {
  # $1 = cartella di staging (già creata)
  local stage="$1" triple deps d n inc_n lib_n
  triple="$(rid_triple "$RID")" || { echo "ERRORE: RID non valido: $RID" >&2; return 1; }
  deps="${DEPS_DIR:-$HOME/gmb-deps/$triple}"
  if [ ! -d "$deps/include" ] || [ ! -d "$deps/lib" ]; then
    echo "ERRORE: mancano le terze parti per $RID in $deps" >&2
    echo "  generarle (una volta per target): tools/c/build-deps.sh --target $triple" >&2
    return 1
  fi
  mkdir -p "$stage/include" "$stage/lib" || return 1

  # 1) terze parti: albero degli header + archivi (già per il triple giusto, nessun filtro)
  ( cd "$deps/include" && tar cf - . ) | ( cd "$stage/include" && tar xf - ) || return 1
  cp -f "$deps"/lib/*.a "$deps"/lib/*.dylib "$deps"/lib/*.so* "$deps"/lib/*.lib "$stage/lib/" 2>/dev/null

  # 1bis) RID Windows: lo strato POSIX→Win32 (`GMBasic/native/win32shim/include`) è parte del
  #    payload di compilazione, non un extra: senza, `sys/socket.h`, `unistd.h`, `pthread.h`,
  #    `netdb.h` non risolvono e i sorgenti — che sono gli STESSI di macOS/Linux — non compilano.
  case "$RID" in
    win-*)
      ( cd "$ROOT/native/win32shim/include" && tar cf - . ) | ( cd "$stage/include" && tar xf - ) || return 1 ;;
  esac

  # 2) header NOSTRI, piatti: i sorgenti li includono per nome (`#include "gmb_runtime.h"`)
  for d in "$ROOT"/native/*/include; do
    [ -d "$d" ] || continue
    cp -f "$d"/*.h "$stage/include/" 2>/dev/null
  done

  # 2bis) header INTERNI di progetto (`src/*.h`, `*.inc`): non sono API pubblica, ma senza
  #    `gmb_hpack_huffman.h` o `gmb_tui_*.inc` il runtime non si ricompila. `cp -n`: l'API
  #    pubblica (`include/`) vince sempre sui nomi uguali.
  for d in "$ROOT"/native/*/src; do
    [ -d "$d" ] || continue
    cp -n "$d"/*.h   "$stage/include/" 2>/dev/null
    cp -n "$d"/*.inc "$stage/include/" 2>/dev/null
  done

  # 3) header GENERATI nel build: `gmb_gfx_vulkan.c` include `gmb_gfx_spirv.h`, che non è nel
  #    repo. Senza, quel sorgente non compila e il gate lo segnala come mancante.
  for d in "$HOME/gmb-deps/gen" "$ROOT/build/cmake-$RID" "$ROOT/native/gmb_runtime_c/build"; do
    [ -f "$d/gmb_gfx_spirv.h" ] && cp -f "$d/gmb_gfx_spirv.h" "$stage/include/"
  done

  # 4) statiche NOSTRE del RID (cross: <root>/build/cmake-<rid>; host: <progetto>/build).
  #    `libgmb_runtime_min.a` è la variante minima del runtime: non è il payload di compilazione.
  local roots=("$ROOT/build/cmake-$RID" "$ROOT/build/libs/$RID")
  for d in gmb_runtime_c gmb_compiler_c gmb_bytecode gmb_db_c gmb_doc_c gmb_chart gmb_agent_runtime; do
    roots+=("$ROOT/native/$d/build")
  done
  for d in "${roots[@]}"; do
    [ -d "$d" ] || continue
    while IFS= read -r f; do
      case "$(basename "$f")" in libgmb_runtime_min.a) continue;; esac
      cp -f "$f" "$stage/lib/" 2>/dev/null
    done < <(find "$d" -maxdepth 2 -type f \( -name 'lib*.a' -o -name 'lib*.dll.a' -o -name '*.lib' \
                 -o -name 'lib*.dylib' -o -name 'lib*.so' -o -name 'lib*.so.*' \) 2>/dev/null)
  done

  inc_n=$(find "$stage/include" -type f | wc -l | tr -d ' ')
  lib_n=$(find "$stage/lib" -type f | wc -l | tr -d ' ')
  n=$((inc_n + lib_n))
  echo "   dev $RID: $inc_n header, $lib_n archivi (tot $n file) · deps=$deps"
  [ "$n" -gt 0 ] || { echo "ERRORE: staging dev vuoto" >&2; return 1; }
  return 0
}


if [ "$DO_BUILD" = "1" ] && [ "$GROUP" = "dev" ]; then
  # `dev` non produce nulla: raccoglie le statiche/header già costruiti per il RID (build_core
  # ricompilerebbe le librerie DINAMICHE, che non fanno parte di questo payload).
  echo "== 1) compilazione saltata (dev: uso gli artefatti già presenti per $RID)"
elif [ "$DO_BUILD" = "1" ]; then
  if [ "$ENGINE_LOCAL" = "1" ]; then
    echo "== 1) compilazione ($GROUP, tramite i sottoprogetti LLMEnv) =="
    build_engine_local || { echo "ERRORE: build fallita" >&2; exit 1; }
  else
    echo "== 1) compilazione (core) =="
    build_core || { echo "ERRORE: build fallita" >&2; exit 1; }
  fi
else
  echo "== 1) compilazione saltata (--no-build) =="
fi

# ── STAGING: raccolta per regole di gruppo, versione più recente per nome ───────────────────
TMP_STAGE=0
if [ -n "$STAGE_DIR" ]; then
  STAGE="$STAGE_DIR"; mkdir -p "$STAGE"
else
  STAGE="$(mktemp -d "${TMPDIR:-/tmp}/gmblibs-stage.XXXXXX")"; TMP_STAGE=1
fi
GATE_DIR="$STAGE"
trap '[ "$TMP_STAGE" = "1" ] && rm -rf "$STAGE"; [ "$GATE_DIR" != "$STAGE" ] && rm -rf "$GATE_DIR"; true' EXIT

# `dev` non passa dallo staging delle librerie dinamiche (vedi stage_dev): si mette in stage il
# payload di COMPILAZIONE e si misura col suo gate. La pubblicazione resta al canale unico degli
# asset (i path `include/…` diventano `include__…` con libs_publish_files_gh.sh), quindi qui si
# stampa il comando esatto e non si pubblica: `--publish` su `dev` è volutamente non supportato.
if [ "$GROUP" = "dev" ]; then
  stage_dev "$STAGE" || exit 1
  if [ "$DO_VERIFY" = "1" ] && [ "$DRY" = "0" ]; then
    echo "== 3) gate dev (libs_dev_selfcheck.sh)"
    bash "$SCRIPT_DIR/libs_dev_selfcheck.sh" "$STAGE" --rid "$RID" || {
      echo "ERRORE: il bundle dev non basta a compilare per $RID" >&2; exit 1; }
  fi
  echo "== 4) pubblicazione: NON in questo script (asset piatti, un solo writer)"
  echo "   ls -l \"$STAGE/include\" \"$STAGE/lib\""
  echo "   tools/sh/libs_publish_files_gh.sh dev $RID --source \"$STAGE\" --tag dev-v1 --yes"
  [ "$TMP_STAGE" = "1" ] && [ "$DRY" = "0" ] && echo "   (staging temporaneo: usare --stage-dir per conservarlo)"
  exit 0
fi

ROOTS=(
  "$ROOT/build/libs/$RID"
  # CACHE utente: è lì che vivono le TERZE PARTI (support) e le lib scaricate da GMBasicModules.
  # Serve ai gruppi che le incorporano (es. `runtime` = boot+core+engine+support): senza, lo
  # staging avrebbe solo i nostri artefatti di build e il bundle resterebbe incompleto.
  "$HOME/.gmbasic/libs/$RID"
)
for pr in gmb_runtime_c gmb_compiler_c gmb_sql_ext gmb_torch; do
  d="$(build_dir "$ROOT/native/$pr")"
  [ -d "$d" ] && ROOTS+=("$d")
done
# engine/llmmotor: i file sono nei sottoprogetti (llmcodec_build/, llmtoken_build/, …) → ricorsivo
STAGE_DEPTH=1
# `runtime` incorpora anche i sottoprogetti LLMEnv, che scrivono in `<build>/llmtoken_build/` e
# `<build>/llmcodec_build/` (profondità 2): con depth 1 il bundle restava senza libllmtoken.so,
# e `libgmb_ai_llm.so`/`libgmb_grep.so` non erano caricabili ("libllmtoken.so: cannot open").
[ "$GROUP" = "runtime" ] || [ "$GROUP" = "runtime-core" ] && STAGE_DEPTH=2
[ "$ENGINE_LOCAL" = "1" ] && STAGE_DEPTH=6
echo "== 2) staging in $STAGE =="
for r in "${ROOTS[@]}"; do
  [ -d "$r" ] || continue
  while IFS= read -r f; do
    [ -n "$f" ] || continue
    base="$(basename "$f")"
    picked="$(libs_group_pick "$GROUP" "$base")"
    [ -n "$picked" ] || continue
    # la copia più recente vince (un build vecchio non deve sovrascrivere quello appena fatto)
    if [ -f "$STAGE/$base" ] && [ ! "$f" -nt "$STAGE/$base" ]; then continue; fi
    cp -f "$f" "$STAGE/$base"
  done < <(find "$r" -maxdepth "$STAGE_DEPTH" -type f \( -name "*.dylib" -o -name "*.so" -o -name "*.so.*" -o -name "*.dll" \) | sort)
done

COUNT=$(find "$STAGE" -maxdepth 1 -type f | wc -l | tr -d ' ')
echo "   file raccolti: $COUNT"
if [ "$COUNT" = "0" ]; then echo "ERRORE: nessun file del gruppo '$GROUP' negli alberi di build" >&2; exit 1; fi
find "$STAGE" -maxdepth 1 -type f -exec basename {} \; | sort | sed 's/^/     /'

# ── EXPORT MANIFEST: il bundle DICHIARA cosa le sue dylib cercano a load-time ───────────────────
# Le dylib nostre sono linkate con `-undefined dynamic_lookup`: i simboli del core li risolvono
# quando dyld le carica, e chi li fornisce è la tabella degli export dell'eseguibile. La lista vive
# accanto alle dylib (`libgmb.exports.txt`), scritta qui e copiata nel bundle: così il requisito
# viaggia con le dylib e il link di un programma non può restare indietro (il runner la legge, e
# dopo il link verifica che l'eseguibile esporti tutto: se manca qualcosa si ferma, invece di
# consegnare un binario che crasha all'avvio). Solo Darwin: su altre piattaforme gli import
# flat-namespace non esistono e gli export li decide il linker (`--export-dynamic`, .def).
if [ "$(uname -s)" = Darwin ] && [ "${DRY:-0}" != "1" ]; then
  if [ -f "$SCRIPT_DIR/libs_exports_file.sh" ]; then
    bash "$SCRIPT_DIR/libs_exports_file.sh" write "$ROOT/build/libs/$RID" || \
      echo "   ATTENZIONE: libgmb.exports.txt non rigenerato" >&2
    if [ -f "$ROOT/build/libs/$RID/libgmb.exports.txt" ]; then
      cp -f "$ROOT/build/libs/$RID/libgmb.exports.txt" "$STAGE/"
      echo "   export manifest: $(wc -l < "$STAGE/libgmb.exports.txt" | tr -d ' ') simboli"
    fi
  fi
fi

# ── COPERTURA: il set deve coprire ciò che l'indice dichiara per questo RID ─────────────────
# Un file elencato nell'indice e NON prodotto dal build = bundle irriproducibile: si blocca
# (niente pubblicazione a metà) e si dice esattamente quale progetto aggiungere.
if [ -f "$MANIFEST" ] && command -v jq >/dev/null 2>&1; then
  MISSING=""
  while IFS= read -r n; do
    [ -n "$n" ] || continue
    [ -f "$STAGE/$n" ] || MISSING="$MISSING $n"
  done < <(jq -r --arg r "$RID" --arg g "$GROUP" \
             '(.rids[$r].files // {}) | to_entries[] | select(.value.group==$g) | .key' "$MANIFEST")
  if [ -n "$MISSING" ]; then
    echo "== COPERTURA: file nell'indice senza produttore =="
    for n in $MISSING; do echo "   MANCA: $n"; done
    if [ "$ALLOW_GAPS" = "1" ]; then
      echo "   (--allow-gaps: pubblico comunque — il bundle NON sarà completo)"
    else
      echo "ERRORE: bundle incompleto. Aggiungi il progetto/target produttore in build_core" >&2
      echo "        (o usa --allow-gaps per pubblicare consapevolmente un set ridotto)." >&2
      exit 1
    fi
  else
    echo "== COPERTURA: il set copre tutti i file dell'indice per $GROUP/$RID =="
  fi
else
  echo "== COPERTURA: manifest o jq assenti, salto il confronto =="
fi

# ── GATE: ogni libreria deve caricarsi (dlopen in processo separato) ────────────────────────
# Il gate deve vedere la disposizione REALE della cache: `core` linka il gruppo BASE `engine`
# (libllmtoken, libllmcodec_*). Nella cache stanno nella stessa cartella; qui si aggiungono in
# una cartella di gate separata (solo per il dlopen: NON entrano nel bundle pubblicato).
if [ "$GROUP" = "core" ]; then
  GATE_DIR="$STAGE.gate"
  rm -rf "$GATE_DIR"; mkdir -p "$GATE_DIR"; cp -R "$STAGE"/. "$GATE_DIR"/
  for r in "${ROOTS[@]}"; do
    [ -d "$r" ] || continue
    while IFS= read -r f; do
      b="$(basename "$f")"
      [ -n "$(libs_group_pick engine "$b")" ] || continue
      [ -e "$GATE_DIR/$b" ] || ln -sf "$f" "$GATE_DIR/$b"
    done < <(find "$r" -type f \( -name "*.dylib" -o -name "*.so" -o -name "*.so.*" -o -name "*.dll" \) | sort)
  done
fi

if [ "$DO_VERIFY" = "1" ]; then
  echo "== 3) gate di caricabilità (libs_verify_load.sh) =="
  if [ "$DRY" = "1" ]; then
    echo "   (dry-run: salto il gate)"
  else
    bash "$SCRIPT_DIR/libs_verify_load.sh" "$GATE_DIR" || { echo "ERRORE: gate fallito (dipendenza mancante nel bundle)" >&2; exit 1; }
  fi
fi

# ── PUBBLICAZIONE ───────────────────────────────────────────────────────────────────────────
if [ "$PUBLISH" != "1" ]; then
  echo "== 4) pubblicazione NON richiesta (--publish per upload + indice) =="
  echo "   per ispezionare: ls -l $STAGE"
  [ "$DRY" = "1" ] && trap - EXIT   # in dry-run lascia la cartella per ispezione
  exit 0
fi
[ "$DRY" = "1" ] && { echo "== 4) dry-run: niente upload =="; exit 0; }

args=("$GROUP" "$RID" --source "$STAGE")
[ -n "$TAG" ] && args+=(--tag "$TAG")
[ "$YES" = "1" ] && args+=(--yes)
args+=(--index --push-manifest)
echo "== 4) pubblicazione: libs_publish_gh.sh ${args[*]} =="
bash "$SCRIPT_DIR/libs_publish_gh.sh" "${args[@]}"
