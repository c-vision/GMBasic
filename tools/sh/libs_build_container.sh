#!/usr/bin/env bash
# libs_build_container.sh — produce i bundle nativi di Linux SENZA una macchina Linux:
# esegue la STESSA pipeline (`tools/sh/libs_build_release.sh`) dentro un container Ubuntu.
#
# PERCHÉ ESISTE. I bundle `core`/`engine`/`llmmotor` erano pubblicati solo per osx-arm64: i
# binari vanno compilati per OS e su macOS non si compilano `.so` Linux. Il container dà
# l'ambiente Linux su questa macchina, con l'unica differenza che i file escono su un volume
# montato (`--stage-dir`) invece che in /tmp del container.
#
# COSA FA
#   1. controlla il daemon Docker (e lo avvia se richiesto)
#   2. container ubuntu con: repo montato in /src, LLMEnv in /llmenv, stage in /out
#   3. installa le dipendenze di sistema, poi `tools/sh/libs_build_release.sh <gruppo>
#      --rid <rid> --build-root /build --stage-dir /out`
#   4. lascia i file in <out> (default: ~/gmb-bundles/<rid>) — il gate di caricabilità gira
#      DENTRO il container (è lì che deve essere verde)
#
# USO
#   tools/sh/libs_build_container.sh <gruppo> [opzioni]
#     gruppo: runtime (default consigliato) | core | engine | llmmotor | support
#             (engine/llmmotor richiedono LLMEnv montato)
#     --rid linux-arm64|linux-x64   default: linux-<arch della macchina>
#     --out DIR                     default: $HOME/gmb-bundles/<rid>
#     --image IMG                   default: ubuntu:24.04 (glibc 2.35: compatibile con distro 2022+)
#     --no-build                    solo staging+gate (usa artefatti già presenti)
#     --allow-gaps                  passa --allow-gaps alla pipeline
#     --dry-run                     stampa il comando docker e non esegue
#     --keep                        non rimuove il container a fine run (diagnosi)
#
# ESEMPI
#   tools/sh/libs_build_container.sh core                        # linux-arm64 su Mac Apple Silicon
#   tools/sh/libs_build_container.sh core --rid linux-x64        # emulazione amd64 (più lento)
#   tools/sh/libs_build_container.sh engine --rid linux-arm64
#
# NOTA. Non pubblica: produzione e pubblicazione restano separati. Dopo un run verde:
#   tools/sh/libs_publish_gh.sh core <rid> --source <out> --index --yes
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../GMBasic" && pwd)"          # <monorepo>/GMBasic
MONO="$(cd "$ROOT/.." && pwd)"                # <monorepo>

GROUP="${1:-core}"; [ $# -gt 0 ] && shift
RID=""; OUT=""; IMAGE="ubuntu:24.04"; NO_BUILD=0; ALLOW_GAPS=0; DRY=0; KEEP=0
while [ $# -gt 0 ]; do
  case "$1" in
    --rid) RID="${2:-}"; shift 2;;
    --out) OUT="${2:-}"; shift 2;;
    --image) IMAGE="${2:-}"; shift 2;;
    --no-build) NO_BUILD=1; shift;;
    --allow-gaps) ALLOW_GAPS=1; shift;;
    --dry-run) DRY=1; shift;;
    --keep) KEEP=1; shift;;
    -h|--help) sed -n '2,40p' "$0" | sed 's/^# \{0,1\}//'; exit 0;;
    *) echo "opzione sconosciuta: $1" >&2; exit 2;;
  esac
done
# `runtime` è il gruppo CHIUSO che serve a ESEGUIRE (boot+core+engine+support): la pipeline
# `libs_build_release.sh` lo conosce, questo wrapper era rimasto ai gruppi storici.
case "$GROUP" in runtime|runtime-core|boot|core|engine|llmmotor|support) ;; *) echo "gruppo non valido: $GROUP" >&2; exit 2;; esac

if [ -z "$RID" ]; then
  case "$(uname -m)" in
    arm64|aarch64) RID=linux-arm64;;
    x86_64)        RID=linux-x64;;
    *) echo "arch non riconosciuta: passare --rid" >&2; exit 2;;
  esac
fi
# un RID può avere PIÙ gruppi: senza sottocartella, il bundle di `engine` finirebbe mescolato a
# quello di `core` (e il publish, che filtra per gruppo, funzionerebbe lo stesso ma la cartella
# diventerebbe ambigua: quale file viene da quale run?).
[ -n "$OUT" ] || OUT="$HOME/gmb-bundles/$RID/$GROUP"
mkdir -p "$OUT"

# LLMEnv (sorgenti del motore AI: servono anche a `core`, che linka libllmcodec/llmtoken statici)
LLMENV_HOST=""
for c in "${LLMENV:-}" "$MONO/LLMEnv" "$MONO/../LLMEnv" "$HOME/Documents/Projects/LLMEnv"; do
  [ -n "$c" ] && [ -d "$c/LLMCodec" ] && { LLMENV_HOST="$c"; break; }
done
if [ -z "$LLMENV_HOST" ]; then
  echo "ERRORE: LLMEnv non trovato (serve per i link statici di llmcodec/llmtoken)." >&2
  echo "        Indica la cartella con LLMENV=/percorso/LLMEnv." >&2
  exit 1
fi

# daemon
if ! docker info >/dev/null 2>&1; then
  echo "daemon Docker non attivo: provo ad avviare Docker Desktop…"
  open -a Docker 2>/dev/null || { echo "ERRORE: avvia Docker manualmente." >&2; exit 1; }
  for _ in $(seq 1 30); do docker info >/dev/null 2>&1 && break; sleep 3; done
  docker info >/dev/null 2>&1 || { echo "ERRORE: daemon Docker non raggiungibile." >&2; exit 1; }
fi

PLATFORM_ARG=""
case "$RID" in
  linux-arm64) PLATFORM_ARG="--platform linux/arm64";;
  linux-x64)   PLATFORM_ARG="--platform linux/amd64";;   # emulazione qemu: lento ma corretto
  *) echo "RID non Linux: $RID (usa tools/sh/libs_build_release.sh nativo)" >&2; exit 2;;
esac

# Dipendenze di sistema. Elenco PRUDENTE: il primo run serve a scoprire cosa manca davvero
# (le soddisfatte in più non fanno danno). `pkg-config` serve a trovare SDL/zlib/openssl.
DEPS="build-essential cmake pkg-config git ca-certificates unzip zip jq \
      libssl-dev zlib1g-dev libbz2-dev libzstd-dev liblz4-dev libbrotli-dev \
      libcurl4-openssl-dev libsqlite3-dev libsdl2-dev libsdl2-ttf-dev \
      libjpeg-dev libpng-dev libtiff-dev libwebp-dev libopenblas-dev libgomp1 curl"

INNER_ARGS=("$GROUP" "--rid" "$RID" "--build-root" "/build" "--stage-dir" "/out")
[ "$NO_BUILD" = "1" ] && INNER_ARGS+=("--no-build")
[ "$ALLOW_GAPS" = "1" ] && INNER_ARGS+=("--allow-gaps")

NAME="gmb-libs-$RID-$$"
DOCKER_CMD=(docker run --rm $PLATFORM_ARG --name "$NAME"
  -v "$MONO":/src -v "$LLMENV_HOST":/llmenv:ro -v "$OUT":/out
  -e LLMENV=/llmenv -w /src "$IMAGE"
  bash -lc "set -euo pipefail
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -qq
    apt-get install -y -qq --no-install-recommends $DEPS
    echo '--- bootstrap: terze parti del RID nella cache ---'
    bash /src/tools/sh/libs_bundle_bootstrap.sh $RID || true
    echo '--- header di sviluppo nella cache (gruppo dev locale) ---'
    bash /src/tools/sh/libs_cache_headers_linux.sh $RID
    echo '--- build dentro il container ---'
    # NB: gli script vivono nella RADICE del monorepo (/src/tools/sh), non piu' in
    # /src/GMBasic/tools: la CWD era quella vecchia e il container moriva con
    # errore 127 (script non trovato).
    # NB2: in questo blocco NIENTE apici inversi ne' doppi apici: e' una stringa fra
    # doppi apici dell'outer script e verrebbero interpretati (command substitution /
    # chiusura anticipata della stringa).
    bash /src/tools/sh/libs_build_release.sh ${INNER_ARGS[*]}")

echo "== libs_build_container: gruppo=$GROUP rid=$RID immagine=$IMAGE"
echo "   repo:   $MONO  → /src"
echo "   llmenv: $LLMENV_HOST → /llmenv"
echo "   out:    $OUT → /out"
if [ "$DRY" = "1" ]; then
  printf '   comando:'; printf ' %q' "${DOCKER_CMD[@]}"; echo; exit 0
fi

if [ "$KEEP" = "1" ]; then
  # senza --rm, per poter entrare nel container dopo un fallimento
  DOCKER_CMD=("${DOCKER_CMD[@]/--rm/}")
fi

"${DOCKER_CMD[@]}"
rc=$?
echo "== esito: $rc =="
if [ "$KEEP" = "1" ] && [ "$rc" != "0" ]; then
  echo "container conservato: docker exec -it $NAME bash"
fi
if [ "$rc" = "0" ]; then
  echo "file prodotti in: $OUT"
  ls -l "$OUT" | head -20
  echo
  echo "pubblicazione (quando sei soddisfatto):"
  echo "  $SCRIPT_DIR/libs_publish_gh.sh $GROUP $RID --source \"$OUT\" --index --yes"
fi
exit $rc
