#!/usr/bin/env bash
# libs_publish_gh.sh — pubblica UN GRUPPO di librerie native su c-vision/GMBasicModules
# (release GitHub) e, con --index, aggiorna `libs-manifest.json` (l'indice che il resolver legge).
#
# PERCHÉ ESISTE. Le librerie native non stanno più nei repo: stanno in un unico posto
# (c-vision/GMBasicModules) e si scaricano ON-DEMAND in `~/.gmbasic/libs/<rid>/`. Chi PRODUCE una
# libreria la pubblica con questo script, su comando:
#
#   support  (B) terze parti      SDL, freetype, harfbuzz, pdf_oxide, tesseract/leptonica/tessdata,
#                                 png/jpeg/tiff/webp/avif, audio, crypto/ssl, hdf5 …
#                                 sorgente: bundle curato
#   engine   (C) *plumbing* AI    `libllmcodec_*` (17), `libllmtoken`, `libllm_vision_apple`,
#                                 `libllmengine_moe_stream`, `libgmb_codec_*` — ~0,5 MB.
#                                 OGNI dylib nostra li linka: senz'altro `libgmb_gfx`/`libgmb_pkg`/
#                                 `libgmb_compress` NON si caricano ("Library not loaded") → gruppo BASE.
#   llmmotor (C) motori AI        `libllm_ai_mlx` + `mlx.metallib`, `libllm_ai_llamacpp`
#                                 (llama.cpp STATICO dentro), wrapper `libgmb_ai_mlx`/`_moestream`/
#                                 `_finetune` — ~50 MB: serve SOLO per l'inferenza locale (nessuna
#                                 dylib base la linka). NON si spediscono `libllama*`/`libggml*`/
#                                 `libmtmd*` (orfani: li linkava solo `libgmb_ai_llamacpp`, mai caricato,
#                                 con rpath assoluti verso un albero di build inesistente).
#                                 sorgente: build di LLMEnv / dist di FastLLM
#                                 → `LLMEnv/tools/publish_libs_gmbasicmodules.sh` fa lo staging
#                                 e pubblica ENTRAMBI i gruppi.
#   core     (D) build di GMBasic `libgmb_ai_llm`/`_modelio`, `libgmb_torch(+loader)`, `libgmb_tray`,
#                                 `libgmb_pkg`, `libgmb_gfx`, `libgmb_compress/grep`, `gmb_sql_*_ext`
#                                 sorgente: build CMake di GMBasic
#
# GRANULARITÀ: un asset per GRUPPO e per RID (support ~30 MB, engine ~0,5 MB, llmmotor ~64 MB,
# core ~3 MB), così chi ha bisogno di SDL non scarica 64 MB di MLX. `libs_manifest_gh.sh` mappa OGNI
# file al suo gruppo.
#
# `torch` NON si pubblica qui: resta su pytorch.org ed è il gruppo `external` del manifest (lo
# costruisce `libs_manifest_gh.sh` leggendo `shared/settings/ml-manifest.json`).
#
# USO
#   tools/sh/libs_publish_gh.sh <support|engine|core> <rid> [opzioni]
#     --source DIR[,DIR]  cartella/e delle lib (default: build/libs/<rid>,~/.gmbasic/libs/<rid>)
#     --tag TAG        release tag (default: gmb-<gruppo>-<YYYY.MM.DD>)
#     --repo OWNER/REPO  default: c-vision/GMBasicModules
#     --dry-run        crea lo zip, stampa file/sha/size, nessun upload
#     --yes            non chiede conferma prima dell'upload
#     --index          dopo l'upload rigenera `libs-manifest.json` (tools/sh/libs_manifest_gh.sh build)
#     --push-manifest  come --index, e pubblica il manifest su `main`
#     --manifest FILE  percorso del manifest (default: <monorepo>/shared/settings/libs-manifest.json)
#
# ESEMPI
#   # core (artefatti del build GMBasic)
#   tools/sh/libs_publish_gh.sh core osx-arm64 --index
#
#   # engine (di norma lo lancia LLMEnv, che fa lo staging)
#   LLMEnv$ tools/publish_libs_gmbasicmodules.sh osx-arm64 --push-manifest
set -euo pipefail

GROUP="${1:-}"; RID="${2:-}"
if [ -z "$GROUP" ] || [ -z "$RID" ]; then sed -n '2,45p' "$0" | sed 's/^# \{0,1\}//'; exit 2; fi
case "$GROUP" in support|engine|llmmotor|core) ;; *) echo "gruppo non valido: $GROUP (support|engine|llmmotor|core)" >&2; exit 2;; esac

shift 2
SRC=""; TAG=""; REPO="c-vision/GMBasicModules"; DRY=0; YES=0; INDEX=0; PUSH=0; MANIFEST=""
while [ $# -gt 0 ]; do
  case "$1" in
    --source) SRC="${2:-}"; shift 2;;
    --tag) TAG="${2:-}"; shift 2;;
    --repo) REPO="${2:-}"; shift 2;;
    --dry-run) DRY=1; shift;;
    --yes) YES=1; shift;;
    --index) INDEX=1; shift;;
    --push-manifest) INDEX=1; PUSH=1; shift;;
    --manifest) MANIFEST="${2:-}"; shift 2;;
    *) echo "opzione sconosciuta: $1" >&2; exit 2;;
  esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../GMBasic" && pwd)"
[ -n "$MANIFEST" ] || MANIFEST="$ROOT/../shared/settings/libs-manifest.json"
if [ -z "$SRC" ]; then
  # più sorgenti (prima vince sul nome): i nostri artefatti stanno in build/libs, le terze parti
  # (compress/grep) stanno nel build o nella cache → si passano entrambe.
  a="$ROOT/build/libs/$RID"; b="$HOME/.gmbasic/libs/$RID"
  if [ -d "$a" ] && [ -d "$b" ]; then SRC="$a,$b"; elif [ -d "$a" ]; then SRC="$a"; else SRC="$b"; fi
fi
[ -n "$SRC" ] || { echo "sorgente non trovata per RID $RID" >&2; exit 1; }
[ -n "$TAG" ] || TAG="gmb-$GROUP-$(date +%Y.%m.%d)"

# ── selezione dei file per gruppo (unica definizione; il manifest usa le stesse regole) ──────────
# engine : runtime AI — wrapper libgmb_ai_{mlx,llamacpp,moestream} + libgmb_codec_*, MLX (+metallib),
#          llama.cpp/ggml/mtmd, codec ML, tokenizer, vision.
# core   : librerie PROPRIE di GMBasic (build CMake): ai_llm/modelio/finetune, torch wrapper,
#          tray, pkg, gfx, compress/grep, sql ext.
# support: tutto ciò che NON è nostro e NON è motore AI e NON è torch.
# Regole di appartenenza: UNICA definizione in tools/sh/libs_groups.sh (condivisa con
# libs_build_release.sh e libs_manifest_gh.sh). Qui solo l'alias storico `pick`.
# shellcheck source=libs_groups.sh
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/libs_groups.sh"
LIBS_RID="$RID"                      # le regole di gruppo guardano il RID di DESTINAZIONE
pick() { libs_group_pick "$@"; }

# NOTA: niente `mapfile` — macOS ha bash 3.2. Il ciclo while-read è portabile.
# L'estensione accetta i soname Linux (`libfoo.so.1`) oltre a .dylib/.so/.dll/.metallib.
IFS=',' read -r -a SRC_LIST <<< "$SRC"
CAND=()
for s in "${SRC_LIST[@]}"; do
  [ -d "$s" ] || { echo "sorgente non trovata: $s" >&2; exit 1; }
  while IFS= read -r _f; do [ -n "$_f" ] && CAND+=("$_f"); done < <(pick "$GROUP" $(cd "$s" && ls -1 | grep -E '\.(dylib|so(\.[0-9]+)*|dll|metallib)$|^tessdata$' || true))
done

WORK="$(mktemp -d "${TMPDIR:-/tmp}/gmblibs.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

# dedup per nome (il primo sorgente vince)
FILES=(); SEEN="$WORK/.seen"; mkdir -p "$SEEN"
for f in ${CAND[@]+"${CAND[@]}"}; do
  [ -e "$SEEN/$f" ] && continue
  : > "$SEEN/$f"; FILES+=("$f")
done
[ "${#FILES[@]}" -gt 0 ] || { echo "nessun file del gruppo '$GROUP' in $SRC" >&2; exit 1; }

PKGDIR="$WORK/gmb-$GROUP-$RID"; mkdir -p "$PKGDIR"
for f in "${FILES[@]}"; do
  for s in "${SRC_LIST[@]}"; do
    # -f segue il symlink: un link ROTTO viene saltato (mai archiviare link pendenti:
    # finirebbero in cache come voci non risolvibili)
    if [ -f "$s/$f" ]; then cp -RL "$s/$f" "$PKGDIR/"; break; fi
  done
done
ZIP="$WORK/gmb-$GROUP-$RID.zip"
( cd "$WORK" && zip -q -r -9 "$(basename "$ZIP")" "gmb-$GROUP-$RID" )
SHA="$(shasum -a 256 "$ZIP" | awk '{print $1}')"
SIZE="$(stat -f %z "$ZIP" 2>/dev/null || stat -c %s "$ZIP")"
echo "gruppo=$GROUP rid=$RID file=${#FILES[@]} size=$SIZE sha256=$SHA tag=$TAG"

if [ "$DRY" = "1" ]; then
  echo "zip: $ZIP  (dry-run: nessun upload)"; exit 0
fi

if [ "$YES" != "1" ]; then
  printf "Pubblicare su %s (release %s, %s byte)? [y/N] " "$REPO" "$TAG" "$SIZE"
  read -r ans; case "$ans" in y|Y|yes|YES) ;; *) echo "annullato"; exit 0;; esac
fi

if gh release view "$TAG" --repo "$REPO" >/dev/null 2>&1; then
  gh release upload "$TAG" "$ZIP" --repo "$REPO" --clobber
else
  gh release create "$TAG" --repo "$REPO" \
     --title "GMBasic libs — $GROUP $RID" \
     --notes "Bundle $GROUP (gruppo $GROUP) per $RID, generato da tools/libs_publish_gh.sh. $(printf '%s' "${#FILES[@]}") file. Il contratto è libs-manifest.json." \
     "$ZIP"
fi

if [ "$INDEX" = "1" ]; then
  bash "$SCRIPT_DIR/libs_manifest_gh.sh" build --rid "$RID" --repo "$REPO" --manifest "$MANIFEST"
  if [ "$PUSH" = "1" ]; then
    bash "$SCRIPT_DIR/libs_manifest_gh.sh" push --manifest "$MANIFEST" --repo "$REPO"
  fi
fi
