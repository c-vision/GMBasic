#!/usr/bin/env bash
# libs_publish_files_gh.sh — pubblica un gruppo come **UN ASSET PER FILE** (non uno zip),
# con il nome della libreria distinto per sistema operativo:
#
#   <nome-file>-<rid>.<ext>        es:  libgmb_gfx-osx-arm64.dylib
#                                      libllama-linux-x64.so
#
# PERCHÉ. Con un archivio per gruppo un programma che usa UNA libreria ne scaricava l'intero
# gruppo (support = 17-30 MB per 3 MB di librerie). Con un asset per file il download è di quel
# singolo file (1 richiesta, CDN GitHub, sha verificabile) e aggiornare una libreria è
# `gh release upload --clobber` di quel file — senza ingrassare il repository git.
#
# USO
#   tools/sh/libs_publish_files_gh.sh <gruppo> <rid> --source DIR [--tag TAG] [--yes] [--dry-run]
#                                    [--prune | --prune-report]
#
# TAG DI VERSIONE (immutabili): `runtime-v1`, `dev-v1`, `llmmotor-v1`, `support-v1`, `ffi-v1`.
# Su questi tag NON si sovrascrive MAI un asset con contenuto diverso: se serve un cambiamento si
# pubblica `-v2`. È ciò che rende riproducibile un programma compilato: i suoi `<exe>.libs.txt`
# puntano a `-vN` con lo sha esatto, e quel contenuto resta identico per sempre.
#     gruppo: support|core|engine|llmmotor|ffi
#     --source DIR   cartella con i file del gruppo (nome reale: libgmb_gfx.dylib …)
#     --tag TAG      release di destinazione (default: gmb-<gruppo>-<data>)
#     --prune        rimuove dalla release gli asset per-file di questo RID che NON sono più
#                    nella sorgente (default: off; usare quando si rimuove/rinomina una lib)
#     --prune-report elenca su stderr gli asset che --prune rimuoverebbe, SENZA rimuoverli
#                    (implica --prune: nessuna chiamata delete-asset)
#     --yes          non chiede conferma
#     --dry-run      stampa il piano
#
# OUTPUT: una riga TSV per file su stdout — name<TAB>sha256<TAB>size<TAB>url — che è ciò che
# l'indice (`libs_manifest_gh.sh`) e il generatore di `<exe>.libs.txt` consumano.
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "$SCRIPT_DIR/libs_groups.sh"

GROUP="${1:-}"; RID="${2:-}"
[ -n "$GROUP" ] && [ -n "$RID" ] || { sed -n '2,33p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }
shift 2
case "$GROUP" in boot|dev|support|core|engine|llmmotor|ffi|domain|runtime|runtime-core|toolchain) ;; *) echo "gruppo non valido: $GROUP" >&2; exit 2;; esac
LIBS_RID="$RID"   # regole di gruppo per RID di DESTINAZIONE

SRC=""; TAG=""; YES=0; DRY=0; PRUNE=0; PRUNE_REPORT=0; TREE=0; REPO="c-vision/GMBasicModules"
while [ $# -gt 0 ]; do
  case "$1" in
    --source) SRC="${2:-}"; shift 2;;
    --tree) TREE=1; shift;;
    --tag) TAG="${2:-}"; shift 2;;
    --repo) REPO="${2:-}"; shift 2;;
    --prune) PRUNE=1; shift;;
    --prune-report) PRUNE=1; PRUNE_REPORT=1; shift;;
    --yes) YES=1; shift;;
    --dry-run) DRY=1; shift;;
    -h|--help) sed -n '2,33p' "$0" | sed 's/^# \{0,1\}//'; exit 0;;
    *) echo "opzione sconosciuta: $1" >&2; exit 2;;
  esac
done
[ -n "$SRC" ] && [ -d "$SRC" ] || { echo "sorgente non valida: $SRC" >&2; exit 2; }
[ -n "$TAG" ] || TAG="gmb-$GROUP-$(date +%Y.%m.%d)"

# Nome dell'asset: il RID va PRIMA dell'estensione (un file = una libreria, riconoscibile):
#   libgmb_gfx.so → libgmb_gfx-linux-arm64.so      libllama.so.0 → libllama-linux-x64.so.0
#   libgmb_gfx.dylib → libgmb_gfx-osx-arm64.dylib  mlx.metallib → mlx-osx-arm64.metallib
# Separatore di path negli asset del gruppo `dev`: "/" → "__". Un SEGMENTO che inizia o finisce
# con "_" (o che contiene "__") si scontrerebbe col separatore e il path ricostruito sarebbe
# sbagliato (caso reale: include/openssl/__DECC_INCLUDE_PROLOGUE.H → include/openssl//DECC…).
# In quei segmenti soltanto, ogni "_" diventa "-q-" (nessun nome reale contiene "-q-": "-", "q" sono caratteri ammessi da GitHub): tutti gli
# altri nomi restano identici, quindi gli asset già pubblicati non vanno rinominati.
tree_path() { # $1 = path relativo → segmenti uniti con "__", segmenti ambigui escapati
  local out="" seg
  local IFS=/
  for seg in $1; do
    case "$seg" in
      _*|*_|*__*) seg="${seg//_/-q-}";;
    esac
    if [ -n "$out" ]; then out="${out}__$seg"; else out="$seg"; fi
  done
  printf '%s' "$out"
}

# Asset per alberi (dev): il nome porta il PATH RELATIVO con "/" → "__", così `include/SDL2/SDL.h`
# diventa `include__SDL2__SDL-osx-arm64.h`. Il manifest ricostruisce il path invertendo la regola.
asset_name_tree() {
  local rel="$1" stem ext
  case "$rel" in
    *.so|*.so.*) stem="${rel%%.so*}"; ext="${rel#"$stem"}";;
    *.dylib)     stem="${rel%.dylib}"; ext=".dylib";;
    *.dll)       stem="${rel%.dll}";   ext=".dll";;
    *.a)         stem="${rel%.a}";     ext=".a";;
    # header e file di configurazione: il RID va PRIMA dell'estensione finale
    # (include__SDL2__SDL-osx-arm64.h), altrimenti il manifest non ricostruisce il path.
    # QUALUNQUE altra estensione (`include/*` porta .h .hpp .inl .pc .inc .cppm .H …) e le
    # import/statiche di Windows (.lib): il RID va PRIMA dell'estensione finale, altrimenti il
    # manifest non ricostruisce il path (o l'asset non viene indicizzato affatto).
    *.*)         stem="${rel%.*}";     ext=".${rel##*.}";;
    *)           stem="${rel}";        ext="";;
  esac
  printf '%s-%s%s' "$(tree_path "$stem")" "$RID" "$ext"
}

# dispatcher: modalità piatta (runtime) o albero (dev)
A() { if [ "$TREE" = "1" ]; then asset_name_tree "$1"; else asset_name "$1"; fi; }

# sha256 portatile: su Git for Windows `shasum` può mancare (è uno script Perl), mentre `sha256sum`
# e `openssl` ci sono. Con `shasum` assente il confronto diventava "vuoto vs vuoto" e il publisher
# avrebbe saltato per "identico" un asset mai verificato.
sha256_of() { # $1 = file
  if command -v shasum >/dev/null 2>&1; then shasum -a 256 "$1" | awk '{print $1}';
  elif command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | awk '{print $1}';
  else openssl dgst -sha256 "$1" | awk '{print $NF}';
  fi
}

asset_name() {
  local f="$1" stem ext
  # TOOLCHAIN: l'archivio porta GIÀ il RID nel nome (`zig-0.17.0-osx-arm64.tar.xz`); aggiungerlo
  # di nuovo darebbe `…-osx-arm64-osx-arm64.tar.xz`. Il nome del file È il nome dell'asset.
  if [ "$GROUP" = "toolchain" ]; then printf '%s' "$f"; return 0; fi
  case "$f" in
    *.so|*.so.*) stem="${f%%.so*}"; ext="${f#"$stem"}";;
    *.dylib)     stem="${f%.dylib}"; ext=".dylib";;
    *.dll)       stem="${f%.dll}";   ext=".dll";;
    *.metallib)  stem="${f%.metallib}"; ext=".metallib";;
    *)           stem="$f"; ext="";;
  esac
  printf '%s-%s%s' "$stem" "$RID" "$ext"
}

# file del gruppo (stesse regole dell'indice: libs_groups.sh)
FILES=()
while IFS= read -r f; do
  if [ "$TREE" = "1" ]; then b="${f#"$SRC"/}"; else b="$(basename "$f")"; fi
  [ -n "$(libs_group_pick "$GROUP" "$b")" ] || continue
  FILES+=("$b")
done < <(if [ "$TREE" = "1" ]; then find "$SRC" -type f | sort; else find "$SRC" -maxdepth 1 -type f \( -name "*.dylib" -o -name "*.so" -o -name "*.so.*" -o -name "*.dll" -o -name "*.metallib" -o -name "*.tar.xz" \) | sort; fi)
[ "${#FILES[@]}" -gt 0 ] || { echo "nessun file del gruppo '$GROUP' in $SRC" >&2; exit 1; }

echo "== per-file: gruppo=$GROUP rid=$RID tag=$TAG file=${#FILES[@]} (repo=$REPO)" >&2

# ── igiene dell'API GitHub: REST ha un limite orario (5000/h), GraphQL un altro. ───────────────
# Caso reale 2026-10-29: REST esaurita → `gh release view` risponde "release not found" (non
# "rate limit"), il publisher credeva che la release non esistesse, tentava di ricrearla (422
# "tag_name already exists") e poi l'upload falliva. Qui si controlla PRIMA la quota (l'endpoint
# `rate_limit` non consuma) e si esce con la causa vera: "REST esaurita, reset alle HH:MM".
gh_core_left() { gh api rate_limit --jq '.resources.core.remaining' 2>/dev/null || echo "1"; }
gh_core_reset() { gh api rate_limit --jq '.resources.core.reset' 2>/dev/null | { read -r t; [ -n "$t" ] && date -r "$t" '+%H:%M' || echo "?"; }; }
if [ "$DRY" != "1" ]; then
  LEFT="$(gh_core_left)"
  if [ "${LEFT:-0}" -le 2 ] 2>/dev/null; then
    echo "ERRORE: quota REST di GitHub esaurita (usata/h), reset alle $(gh_core_reset)." >&2
    echo "        Il caricamento degli asset usa REST: non si può pubblicare adesso (niente è stato toccato)." >&2
    exit 1
  fi
fi

# Asset di una release via GRAPHQL: name<TAB>sha256. Si usa GraphQL (non `gh release view`) perché
# la REST, oltre al limite basso, con la quota esaurita risponde "release not found" (falso).
release_assets_sha() { # $1 = tag
  gh api graphql --paginate -f query='
    query($owner:String!,$repo:String!,$tag:String!,$endCursor:String){
      repository(owner:$owner,name:$repo){
        release(tagName:$tag){
          releaseAssets(first:100, after:$endCursor){
            pageInfo{ hasNextPage endCursor }
            nodes{ name digest }
          }
        }
      }
    }' -f owner="${REPO%%/*}" -f repo="${REPO##*/}" -f tag="$1" \
    --jq '.data.repository.release.releaseAssets.nodes[] | [.name, ((.digest // "") | sub("^sha256:";""))] | @tsv' 2>/dev/null
}
release_exists() { # $1 = tag
  [ -n "$(gh api graphql -f query='
      query($owner:String!,$repo:String!,$tag:String!){
        repository(owner:$owner,name:$repo){ release(tagName:$tag){ id } }
      }' -f owner="${REPO%%/*}" -f repo="${REPO##*/}" -f tag="$1" \
      --jq '.data.repository.release.id // empty' 2>/dev/null)" ]
}

# release: creata se non esiste (gli asset si aggiungono con --clobber)
if [ "$DRY" != "1" ]; then
  if ! release_exists "$TAG"; then
    echo "== creo la release $TAG ==" >&2
    gh release create "$TAG" --repo "$REPO" \
      --title "GMBasic libs — $GROUP (per-file, tutti i RID)" \
      --notes "Asset per file: <libreria>-<rid>.<ext>. Un file = una libreria aggiornabile con --clobber. L'indice è libs-manifest.json." \
      >/dev/null
  fi
fi

# asset già presenti (per --prune e per saltare gli invariati)
EXISTING=""
IMMUTABLE=0
case "$TAG" in *-v[0-9]*) IMMUTABLE=1;; esac
EXIST_SHA=""
if [ "$DRY" != "1" ]; then
  EXIST_SHA="$(release_assets_sha "$TAG")"
  EXISTING="$(printf '%s\n' "$EXIST_SHA" | awk -F'\t' 'NF{print $1}')"
fi

# sha pubblicato di un asset (vuoto se non c'è)
published_sha() { printf '%s\n' "$EXIST_SHA" | awk -F'\t' -v n="$1" '$1==n{print $2; exit}'; }

# `gh release upload` NON rinomina l'asset: il nome è il basename del file caricato. Quindi si
# preparano copie rinominate `<file>-<rid>` in una cartella temporanea e si caricano quelle.
# TAG IMMUTABILE: se l'asset esiste con lo stesso sha si SALTA; se esiste con sha diverso è un
# errore (mai sovrascrivere: i programmi compilati puntano a quello sha). Serve un tag -vN nuovo.
WORK="$(mktemp -d "${TMPDIR:-/tmp}/gmblibsfiles.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
UPLOAD=()
for f in "${FILES[@]}"; do
  a="$(A "$f")"
  ours="$(sha256_of "$SRC/$f")"
  have="$(published_sha "$a")"
  if [ -n "$have" ]; then
    if [ "$have" = "$ours" ]; then echo "  = $a (già pubblicato, identico)" >&2; continue; fi
    if [ "$IMMUTABLE" = "1" ]; then
      echo "ERRORE: '$a' è già in $TAG con sha diverso." >&2
      echo "        Il tag $TAG è IMMUTABILE (versione): pubblica una -vN nuova invece di sovrascrivere." >&2
      exit 1
    fi
    echo "  ~ $a (sovrascrivo: tag mutabile)" >&2
  else
    echo "  + $a" >&2
  fi
  cp -f "$SRC/$f" "$WORK/$a"
  UPLOAD+=("$WORK/$a")
done
[ "${#UPLOAD[@]}" -gt 0 ] || { echo "niente da caricare: tutti gli asset sono già identici" >&2; }

# DRY-RUN: il piano è già stato stampato sopra. Qui si ESCE prima di toccare la rete — prima
# il controllo era DOPO l'upload, quindi `--dry-run` caricava comunque gli asset (bug reale
# 2026-10-29: il piano "dry" di amqp tentava il caricamento su `ffi-v1`).
if [ "$DRY" = "1" ]; then
  echo "(dry-run: nessun upload)" >&2
  exit 0
fi

# un solo upload (più veloce e atomico rispetto a N chiamate)
if [ "${#UPLOAD[@]}" -gt 0 ] && ! gh release upload "$TAG" "${UPLOAD[@]}" --repo "$REPO" --clobber >/dev/null 2>&1; then
  # diagnostica: riprova file per file per dire QUALE fallisce (limite 2 GB/asset, rete…)
  for u in ${UPLOAD[@]+"${UPLOAD[@]}"}; do
    gh release upload "$TAG" "$u" --repo "$REPO" --clobber >/dev/null 2>&1 \
      || { echo "ERRORE upload di $u" >&2; exit 1; }
  done
fi

# verifica: gli asset devono esistere con il nome <file>-<rid>
NOW="$(release_assets_sha "$TAG" | awk -F'\t' 'NF{print $1}')"
MISSING=0
for f in "${FILES[@]}"; do
  a="$(A "$f")"
  grep -qx "$a" <<< "$NOW" || { echo "ATTESO ma assente: $a" >&2; MISSING=$((MISSING+1)); }
done
[ "$MISSING" = "0" ] || { echo "ERRORE: $MISSING asset non pubblicati col nome atteso" >&2; exit 1; }

# --prune: asset per-file di QUESTO RID che non corrispondono più alla sorgente.
# LINEARITÀ (2026-11-11). Prima, per OGNI asset si riscandiva FILES con una subshell
# `$(A "$f")`: costo O(n²), e su un bundle `dev` (600-1000 asset) il comando non terminava in
# tempi utili (misurato: oltre 290 s su 664 asset, sembrava appeso). Ora l'insieme atteso si
# materializza UNA volta e il confronto è `comm` fra due file ordinati: nessuna subshell per asset.
if [ "$PRUNE" = "1" ]; then
  WANT="$(mktemp)"; HAVE="$(mktemp)"
  for f in "${FILES[@]}"; do A "$f"; printf '\n'; done | sort > "$WANT"
  : > "$HAVE"
  while IFS= read -r a; do
    [ -n "$a" ] || continue
    case "$a" in *"-$RID") ;; *"-$RID".*) ;; *) continue;; esac
    printf '%s\n' "$a" >> "$HAVE"
  done <<< "$NOW"
  sort -o "$HAVE" "$HAVE"
  CAND="$(mktemp)"; comm -13 "$WANT" "$HAVE" > "$CAND"
  if [ "$PRUNE_REPORT" = "1" ]; then
    # solo resoconto: elenca e NON chiama `gh release delete-asset`.
    while IFS= read -r a; do
      [ -n "$a" ] || continue
      echo "  prune-report: $a" >&2
    done < "$CAND"
    echo "prune-report: $(grep -c . "$CAND") asset di $RID da rimuovere — nessuno rimosso" >&2
  else
    while IFS= read -r a; do
      [ -n "$a" ] || continue
      echo "  prune: $a" >&2
      gh release delete-asset "$TAG" "$a" --repo "$REPO" --yes >/dev/null 2>&1
    done < "$CAND"
  fi
  rm -f "$WANT" "$HAVE" "$CAND"
fi

# TSV per l'indice: name, sha256, size, url (sha ricalcolato sui byte caricati)
for f in "${FILES[@]}"; do
  sha=$(sha256_of "$SRC/$f")
  size=$(stat -f %z "$SRC/$f" 2>/dev/null || stat -c %s "$SRC/$f")
  printf '%s\t%s\t%s\t%s\n' "$f" "$sha" "$size" \
    "https://github.com/$REPO/releases/download/$TAG/$(A "$f")"
done
