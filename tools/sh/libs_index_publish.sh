#!/usr/bin/env bash
# libs_index_publish.sh — genera l'INDICE PIATTO per RID (`index-<rid>.txt`) e lo pubblica come
# asset della release STABILE `libs-index` (aggiornata con --clobber, URL mai mutevole).
#
# PERCHÉ. Il bootstrap in C (`libgmb_boot`) non ha un parser JSON: legge un TSV di 4-5 colonne.
# Questo file è il punto d'ingresso unico del downloader:
#
#     <nome-file> \t <sha256> \t <size> \t <url> \t [runtime|dev|component:<nome>]
#
# - `runtime`   ciò che serve a ESEGUIRE (boot/support/core/engine/llmmotor/plugins)
# - `dev`       ciò che serve a COMPILARE (header, statiche nostre)
# - `toolchain` il COMPILATORE (zig): non serve né a eseguire né a linkare, ma a compilare
#               OVUNQUE. Scope proprio, così non entra nel bootstrap `runtime`/`dev` di serie:
#               lo prende chi chiede TUTTO (`gmb libs ensure`, primo uso con `bootstrapScope`).
# - `component:<nome>` torch / chromium / obscura / ocr (produttori esterni)
# - senza tag   = runtime (compatibilità)
#
# Il nome-file è il PATH RELATIVO nella cache (`include/SDL2/SDL.h`, `libgmb_gfx.dylib`).
#
# USO
#   tools/sh/libs_index_publish.sh [--rid RID] [--manifest FILE] [--publish] [--out DIR]
set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../GMBasic" && pwd)"
REPO="c-vision/GMBasicModules"; TAG="libs-index"
MANIFEST="$ROOT/../shared/settings/libs-manifest.json"; RID=""; PUBLISH=0; OUT=""

while [ $# -gt 0 ]; do
  case "$1" in
    --rid) RID="${2:-}"; shift 2;;
    --manifest) MANIFEST="${2:-}"; shift 2;;
    --publish) PUBLISH=1; shift;;
    --out) OUT="${2:-}"; shift 2;;
    -h|--help) sed -n '2,24p' "$0" | sed 's/^# \{0,1\}//'; exit 0;;
    *) echo "opzione sconosciuta: $1" >&2; exit 2;;
  esac
done
[ -f "$MANIFEST" ] || { echo "manifest assente: $MANIFEST" >&2; exit 1; }
[ -n "$OUT" ] || OUT="$(mktemp -d "${TMPDIR:-/tmp}/gmbidx.XXXXXX")"
mkdir -p "$OUT"

RIDS=()
if [ -n "$RID" ]; then RIDS=("$RID"); else
  while IFS= read -r r; do RIDS+=("$r"); done < <(jq -r '.rids|keys[]' "$MANIFEST")
fi

for r in "${RIDS[@]}"; do
  f="$OUT/index-$r.txt"
  # runtime + dev: file del RID con `url`; il gruppo `dev` non esiste ancora nel manifest attuale,
  # quindi tutti i file sono `runtime` (il tag per gli altri scope arriverà con P2).
  # intestazione: versioni dei gruppi (un programma compilato pinna questi tag)
  printf '# requires %s\n' "$(jq -r --arg r "$r" '
      [(.rids[$r].groups // {}) | to_entries[] | select(.value.version != null)
       | "\(.key)-\(.value.version)"] | join(" ")' "$MANIFEST")" > "$f"
  jq -r --arg r "$r" '
    (.rids[$r].files // {}) | to_entries[]
    | select(.value.url != null)
    | [ .key, .value.sha256, (.value.size|tostring), .value.url,
        (if (.value.group // "") == "dev" then "dev"
         elif (.value.group // "") == "toolchain" then "toolchain"
         elif (.value.group // "") == "plugins" then "runtime"
         elif ((.value.group // "") | startswith("components:")) then .value.group
         else "runtime" end) ]
    | @tsv' "$MANIFEST" >> "$f"
  printf "%-12s %s file\n" "$r" "$(wc -l < "$f" | tr -d ' ')"
done

if [ "$PUBLISH" = "1" ]; then
  gh release view "$TAG" --repo "$REPO" >/dev/null 2>&1 || \
    gh release create "$TAG" --repo "$REPO" --title "libs-index (indice piatto per RID)" \
      --notes "TSV per il bootstrap C: nome<TAB>sha256<TAB>size<TAB>url<TAB>scope. Aggiornato con --clobber: URL stabile." >/dev/null
  gh release upload "$TAG" "$OUT"/index-*.txt --repo "$REPO" --clobber >/dev/null \
    && echo "pubblicato: $TAG/index-<rid>.txt"
fi
