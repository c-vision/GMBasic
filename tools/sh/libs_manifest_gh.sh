#!/usr/bin/env bash
# libs_manifest_gh.sh — costruisce `libs-manifest.json` da ciò che è DAVVERO pubblicato su
# c-vision/GMBasicModules, e (con `push`) lo pubblica su `main`.
#
# PERCHÉ ESISTE. Il resolver delle librerie (`LibsResolver` C# / `gmb_libs_ensure` C) non deve
# conoscere né i nomi delle release né la struttura degli zip: legge UN indice, `libs-manifest.json`,
# e da lì sa (a) quale bundle contiene il file che gli serve, (b) dove scaricarlo, (c) che sha256
# deve avere. L'indice si genera dalle release REALI, mai a mano: così sha/size/elenco file non
# possono divergere da ciò che l'utente scarica.
#
# SCHEMA (v2, per-RID: un file può avere sha diversi su RID diversi)
#   {
#     "version": "2026.10.28",
#     "cacheDir": "~/.gmbasic/libs",
#     "rids": {
#       "osx-arm64": {
#         "groups": {
#           "support": { "kind": "github-release", "tag": "...", "rids": { "<rid>": {
#                          "archive": "gmb-support-osx-arm64.zip",
#                          "url": "https://github.com/.../releases/download/<tag>/<archive>",
#                          "sha256": "...", "size": 123, "extractRoot": "" } } },
#           "engine":  { ... },
#           "core":    { ... },
#           "torch":   { "kind": "external", "tag": "pytorch-2.13.0", "rids": { "<rid>": {
#                          "url": "https://download.pytorch.org/libtorch/...",
#                          "archive": "libtorch-macos-arm64-2.13.0.zip",
#                          "extractRoot": "libtorch/lib", "sha256": "", "size": 0 } } }
#         },
#         "files": {
#           "libSDL2.dylib":       { "group": "support", "sha256": "...", "size": 123 },
#           "mlx.metallib":        { "group": "engine",  "sha256": "...", "size": 456 },
#           "libgmb_ai_llm.dylib": { "group": "core",    "sha256": "...", "size": 789 },
#           "libtorch_cpu.dylib":  { "group": "torch",   "sha256": "...", "size": 999 }
#         }
#       }
#     }
#   }
#
# `files` è la mappa che rende possibile il download GRANULARE: chiedi `libSDL2.dylib` → gruppo
# `support` (pochi MB), chiedi `mlx.metallib` → gruppo `engine`. `torch` è l'unico gruppo
# `external`: l'archivio resta su pytorch.org, ma per il resolver è identico a tutti gli altri.
#
# USO
#   tools/sh/libs_manifest_gh.sh build [--rid RID] [--repo OWNER/REPO] [--manifest FILE]
#                                   [--groups support,engine,core] [--no-torch]
#   tools/sh/libs_manifest_gh.sh push  [--manifest FILE] [--repo OWNER/REPO]
#   tools/sh/libs_manifest_gh.sh show  [--manifest FILE] [--rid RID]
#
# DEFAULT: --rid = RID della macchina; --manifest = <monorepo>/shared/settings/libs-manifest.json;
#          --repo = c-vision/GMBasicModules.
#
# ESEMPI
#   # dopo aver pubblicato engine con libs_publish_gh.sh (o dallo script di LLMEnv):
#   tools/sh/libs_manifest_gh.sh build --rid osx-arm64
#   tools/sh/libs_manifest_gh.sh push
#
#   # più RID in un colpo (quando ci saranno i bundle):
#   for r in osx-arm64 linux-x64 win-x64; do tools/sh/libs_manifest_gh.sh build --rid $r; done
#
# NOTA: richiede `gh` autenticato e `jq`. Niente env per configurare i percorsi (regola di
# progetto): si usano i flag.
set -euo pipefail
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/libs_groups.sh"

REPO="c-vision/GMBasicModules"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../GMBasic" && pwd)"
MANIFEST="$ROOT/../shared/settings/libs-manifest.json"   # CONFIG CONDIVISA
TORCH_MANIFEST="$ROOT/../shared/settings/ml-manifest.json"       # CONFIG CONDIVISA (radice monorepo)
RID=""
# Gruppi INDICIZZATI di default. `runtime` (boot+core+engine+support: ciò che serve a ESEGUIRE),
# `llmmotor` (motori AI), `dev` (header/statiche per compilare), `ffi` (le nostre lib caricate con
# DECLARE … LIB: amqp, grpc, kafka, media, mqtt, nats, protobuf) + `torch` (external, sotto).
# I gruppi storici `boot`/`engine`/`core`/`support` NON hanno più una release propria (sono
# confluiti in `runtime-vN`): chiederli faceva fallire il build con "nessuna release trovata".
GROUP_LIST="runtime,llmmotor,dev,ffi,domain"
WITH_TORCH=1
CMD="${1:-build}"; [ $# -gt 0 ] && shift

default_rid() {
  case "$(uname -s)-$(uname -m)" in
    Darwin-arm64) echo "osx-arm64" ;;
    Darwin-x86_64) echo "osx-x64" ;;
    Linux-x86_64) echo "linux-x64" ;;
    Linux-aarch64|Linux-arm64) echo "linux-arm64" ;;
    MINGW*|MSYS*|CYGWIN*) echo "win-x64" ;;
    *) echo "osx-arm64" ;;
  esac
}

while [ $# -gt 0 ]; do
  case "$1" in
    --rid) RID="${2:-}"; shift 2;;
    --repo) REPO="${2:-}"; shift 2;;
    --manifest) MANIFEST="${2:-}"; shift 2;;
    --groups) GROUP_LIST="${2:-}"; shift 2;;
    --no-torch) WITH_TORCH=0; shift;;
    *) echo "opzione sconosciuta: $1" >&2; exit 2;;
  esac
done
[ -n "$RID" ] || RID="$(default_rid)"

command -v jq >/dev/null || { echo "jq richiesto" >&2; exit 1; }
command -v gh >/dev/null || { echo "gh richiesto" >&2; exit 1; }

# ── tag più recente per gruppo (support accetta anche il vecchio schema gmb-libs-*) ──────────────
# L'ordine è per DATA NEL TAG (YYYY.MM.DD), non per `createdAt`: gh riporta per tutte le release
# la data del commit del tag, che qui può essere identica → ordinare per createdAt darebbe la
# release sbagliata.
latest_tag() {
  local g="$1" pat
  case "$g" in
    support) pat='^gmb-(support|libs)-' ;;
    *)       pat="^gmb-$g-" ;;
  esac
  # NON basta il tag più recente: la release di un gruppo può contenere l'asset solo per ALCUNI
  # RID (caso reale 2026-10-29: `gmb-core-2026.10.29` ha solo linux-arm64, `gmb-core-2026.10.28`
  # ha osx-arm64). Scegliendo il tag più recente a prescindere, re-indicizzare osx-arm64 avrebbe
  # CANCELLATO la voce `core` di osx (nessun asset in quel tag).
  # Regola: il tag più recente CHE HA l'asset `gmb-<gruppo>-<RID>.zip`.
  local tags t
  # Due schemi convivono:
  #   - VERSIONE (immutabile): `<gruppo>-v<N>` → si sceglie il numero più alto che ha l'asset
  #     del RID. Non si torna mai indietro e non si sovrascrive: un programma compilato pinna -vN.
  #   - DATA (storico, deprecato): `gmb-<gruppo>-YYYY.MM.DD`, ordinato per data.
  if gh release list --repo "$REPO" --limit 100 --json tagName \
       | jq -r '.[].tagName' | grep -qE "^${g}-v[0-9]+$"; then
    tags="$(gh release list --repo "$REPO" --limit 100 --json tagName -q '.[].tagName' \
              | grep -E "^${g}-v[0-9]+$" \
              | sed -E 's/.*-v([0-9]+)$/\1 &/' | sort -rn | awk '{print $2}')"
  else
    tags="$(gh release list --repo "$REPO" --limit 100 --json tagName \
              | jq -r --arg p "$pat" '
                  map(select(.tagName|test($p)))
                  | map({t:.tagName, d:(.tagName|capture("(?<d>[0-9]{4}\\.[0-9]{2}\\.[0-9]{2})$").d)})
                  | sort_by(.d) | reverse | .[].t')"
  fi
  local assets
  for t in $tags; do
    # il tag è valido per questo RID se ha l'archivio di gruppo OPPURE almeno un asset PER-FILE
    # (`<libreria>-<rid>.<ext>`): con il modello per-file l'archivio non esiste più.
    assets="$(release_assets "$t")" || return 1   # errore API ≠ assenza del gruppo
    if printf '%s\n' "$assets" | jq -Rse --arg a "gmb-$g-$RID.zip" --arg rid "$RID" '
         split("\n") | map(select(length > 0) | split("\t")[0]) as $n
         | ($n | index($a)) != null
           or ($n | map(select(test("-" + $rid + "\\.[A-Za-z0-9.]+$"))) | length > 0)' \
         >/dev/null; then
      echo "$t"; return 0
    fi
  done
  # 3 = nessun tag ha un asset per QUESTO RID: ASSENZA LEGITTIMA, non un errore
  # (misurato 2026-11-10: `dev-v1` ha 431 asset per osx-arm64 e ZERO per osx-x64).
  return 3
}

# ── asset PER-FILE della release per il RID: "asset<TAB>nomefile<TAB>sha256<TAB>size" ──────────
# Formato del nome: <stem>-<rid>.<ext>  (libgmb_gfx-linux-arm64.so, libllama-linux-x64.so.0).
# Il nome "vero" del file si ricostruisce togliendo "-<rid>" prima dell'estensione.
# Asset di una release via GRAPHQL (name<TAB>size<TAB>digest), paginati.
# Perché non `gh release view`: usa la REST API, che ha un limite orario basso e restituisce
# "release not found" quando è esaurito — e il manifest veniva scritto MUTILATO in silenzio.
release_assets() { # $1 = tag · stdout = name<TAB>size<TAB>digest · rc!=0 = ERRORE API (mai muto)
  # L'errore NON si inghiotte (prima c'era `2>/dev/null`): senza distinguere "la release non ha
  # asset per questo RID" da "la query è fallita", il chiamante non poteva fare la cosa giusta —
  # e un fallimento a metà paginazione produceva elenchi TRONCATI (visto: 247 file invece di 95).
  local tag="$1" out err i
  out="$(mktemp)"; err="$(mktemp)"
  for i in 1 2 3; do
    if gh api graphql --paginate -f query='
    query($owner:String!,$repo:String!,$tag:String!,$endCursor:String){
      repository(owner:$owner,name:$repo){
        release(tagName:$tag){
          releaseAssets(first:100, after:$endCursor){
            pageInfo{ hasNextPage endCursor }
            nodes{ name size digest }
          }
        }
      }
    }' -f owner="${REPO%%/*}" -f repo="${REPO##*/}" -f tag="$tag" \
       --jq '.data.repository.release.releaseAssets.nodes[] | [.name, (.size|tostring), (.digest // "")] | @tsv' \
       > "$out" 2> "$err"; then
      cat "$out"; rm -f "$out" "$err"; return 0
    fi
    sleep $((i * 2))   # raffiche di query: GitHub risponde con rate-limit secondario e si rientra
  done
  echo "release_assets($tag): $(head -1 "$err")" >&2
  rm -f "$out" "$err"; return 1
}

assets_perfile() {
  local tag="$1"
  release_assets "$tag" | jq -Rrs --arg rid "$RID" '
    split("\n") | map(select(length > 0) | split("\t") | {name: .[0], size: (.[1]|tonumber?), digest: .[2]})
    | .[]
    | .name as $n
    | (try ($n | capture("^(?<stem>.+)-" + $rid + "(?<ext>\\.[A-Za-z0-9.]+)$")) catch null) as $c
    | select($c != null)
    | [ $n,
        (($c.stem + $c.ext) | gsub("__"; "/")),   # dev: include__SDL2__SDL.h → include/SDL2/SDL.h
        (.digest // "" | sub("^sha256:"; "")), (.size|tostring) ]
    | @tsv' 2>/dev/null
}

# ── indicizza UNA release: ritorna "archive<TAB>sha256<TAB>size<TAB>tag" e scrive i file in $3 ────
index_release() {
  local tag="$1" g="$2" out="$3"
  local work; work="$(mktemp -d "${TMPDIR:-/tmp}/gmblibsidx.XXXXXX")"
  local asset
  asset="$(gh release view "$tag" --repo "$REPO" --json assets \
            -q ".assets[] | select(.name|test(\"^gmb-(support|engine|llmmotor|core|libs)-$RID\\\\.zip$\")) | .name" | head -1)"
  [ -n "$asset" ] || { echo "nessun asset bundle nella release $tag" >&2; rm -rf "$work"; return 1; }
  local meta
  meta="$(gh release view "$tag" --repo "$REPO" --json assets \
            -q ".assets[] | select(.name==\"$asset\") | \"\(.size)\t\(.digest|sub(\"sha256:\";\"\"))\"" | head -1)"
  local size sha
  size="$(printf '%s' "$meta" | cut -f1)"
  sha="$(printf '%s' "$meta" | cut -f2)"

  gh release download "$tag" --repo "$REPO" --pattern "$asset" --dir "$work" >/dev/null
  mkdir -p "$work/x"
  unzip -q "$work/$asset" -d "$work/x"

  : > "$out"
  # toglie la cartella di primo livello SOLO se l'archivio ne ha una sola (e nessun file sciolto)
  local strip_root=0
  local tops
  tops="$(find "$work/x" -maxdepth 1 -mindepth 1 | wc -l | tr -d ' ')"
  if [ "$tops" = "1" ] && [ -d "$(find "$work/x" -maxdepth 1 -mindepth 1 | head -1)" ]; then strip_root=1; fi
  local f rel
  while IFS= read -r f; do
    [ -f "$f" ] || continue
    rel="${f#"$work"/x/}"
    [ "$strip_root" = "1" ] && rel="${rel#*/}"
    case "$rel" in __MACOSX/*|*/.*|.DS_Store|*/.DS_Store) continue;; esac
    jq -cn --arg n "$rel" --arg g "$g" \
      --arg s "$(shasum -a 256 "$f" | awk '{print $1}')" \
      --argjson z "$(stat -f %z "$f" 2>/dev/null || stat -c %s "$f")" \
      '{name:$n, group:$g, sha256:$s, size:$z}'
  done < <(find "$work/x" -type f | sort) >> "$out"

  rm -rf "$work"
  printf '%s\t%s\t%s\t%s\n' "$asset" "$sha" "$size" "$tag"
}

# ── torch: gruppo `external`, l'archivio resta su pytorch.org ────────────────────────────────────
torch_group() {
  [ "$WITH_TORCH" = "1" ] || return 0
  [ -f "$TORCH_MANIFEST" ] || { echo "[torch] manifest assente: $TORCH_MANIFEST (salto)" >&2; return 0; }
  jq -c --arg rid "$RID" '
    .runtimes[$rid] // empty
    | select(.archive.url != null)
    | { ver: (.torchVersion // "unknown"),
        url: .archive.url,
        libDir: (.archive.libDir // "libtorch/lib"),
        files: [ .files[] | select(.source == "archive") | {name, sha256, size} ] }' "$TORCH_MANIFEST"
}

do_build() {
  local work; work="$(mktemp -d "${TMPDIR:-/tmp}/gmblibsman.XXXXXX")"
  trap 'rm -rf "$work"' RETURN

  local groups_json='{}'
  local files_jsonl="$work/files.jsonl"; : > "$files_jsonl"
  local g tag

  # Un gruppo che NON si riesce a indicizzare è un ERRORE, non uno "salto": scrivere il manifest
  # senza quel gruppo lo lascia mutilato (successo reale: rate-limit GitHub → i tre gruppi per-file
  # spariti dall'indice e `gmb libs status` vuoto). Meglio non scrivere nulla.
  local failed=0 rc=0
  for g in ${GROUP_LIST//,/ }; do
    rc=0
    tag="$(latest_tag "$g")" || rc=$?
    if [ "$rc" = "3" ]; then
      # assenza legittima: il gruppo non ha release per questo RID. La voce già nel manifest RESTA
      # (il merge è per gruppo), quindi il RID non si mutila.
      echo "[$g] assente per $RID (nessuna release con asset di questo RID) — la voce precedente resta"
      continue
    fi
    if [ "$rc" != "0" ] || [ -z "$tag" ]; then
      echo "[$g] ERRORE: query delle release fallita (rate-limit GitHub? rete?)" >&2
      failed=1; continue
    fi
    local out="$work/$g.jsonl"; : > "$out"
    LIBS_RID="$RID"   # regole di gruppo per RID di DESTINAZIONE

    # (1) PER-FILE (preferito): la release ha gli asset `<stem>-<rid>.<ext>`.
    #     Nessun download: sha/size arrivano dai metadati dell'asset (digest). È il modello
    #     "una libreria = un file", con URL per singolo file nell'indice.
    # In un FILE, non in process substitution: se la query fallisce a metà il `while` vedrebbe un
    # elenco TRONCATO e scriverebbe un indice parziale in silenzio. Con il file l'errore si vede.
    local nper=0 pf="$work/$g.perfile.tsv"
    if ! assets_perfile "$tag" > "$pf"; then
      echo "[$g] ERRORE: elenco asset non ottenuto ($tag)" >&2
      failed=1; continue
    fi
    while IFS=$'\t' read -r asset name sha size; do
      [ -n "${asset:-}" ] || continue
      # un asset ZIP non è un file di libreria: `gmb-support-linux-arm64.zip` combacia col
      # pattern `<stem>-<rid>.<ext>` e verrebbe indicizzato come file "gmb-support.zip".
      case "$name" in *.zip) continue;; esac
      [ -n "$(libs_group_pick "$g" "$name")" ] || continue
      jq -cn --arg n "$name" --arg g "$g" --arg s "$sha" --argjson z "${size:-0}" \
        --arg u "https://github.com/$REPO/releases/download/$tag/$asset" \
        '{name:$n, group:$g, sha256:$s, size:$z, url:$u}' >> "$out"
      nper=$((nper+1))
    done < "$pf"

    if [ "$nper" -gt 0 ]; then
      cat "$out" >> "$files_jsonl"
      groups_json="$(jq -c --arg g "$g" --arg tag "$tag" --arg rid "$RID" \
          '. + {($g): ({kind:"files", tag:$tag, rids:{($rid): {kind:"files", tag:$tag}}}
                       + (if ($tag|test("-v[0-9]+$")) then {version:($tag|sub("^.*-";""))} else {} end))}' \
          <<< "$groups_json")"
      echo "[$g] $tag · per-file · $nper file"
      continue
    fi

    # (2) ARCHIVIO di gruppo (legacy/compatibilità)
    local meta; meta="$(index_release "$tag" "$g" "$out")" || {
      echo "[$g] ERRORE: indicizzazione fallita ($tag)" >&2; failed=1; continue; }
    local asset sha size
    asset="$(printf '%s' "$meta" | cut -f1)"
    sha="$(printf '%s' "$meta" | cut -f2)"
    size="$(printf '%s' "$meta" | cut -f3)"
    cat "$out" >> "$files_jsonl"
    groups_json="$(jq -c --arg g "$g" --arg tag "$tag" --arg rid "$RID" \
        --arg a "$asset" --arg sha "$sha" --argjson size "${size:-0}" \
        --arg url "https://github.com/$REPO/releases/download/$tag/$asset" \
        '. + {($g): {kind:"github-release", tag:$tag,
                     rids:{($rid): {archive:$a, url:$url, sha256:$sha, size:$size, extractRoot:""}}}}' \
        <<< "$groups_json")"
    echo "[$g] $tag · $asset · $(printf '%s' "$size" | awk '{printf "%.1f MB", $1/1048576}') · $(wc -l < "$out" | tr -d ' ') file"
  done

  # torch (external)
  local tj; tj="$(torch_group)"
  if [ -n "$tj" ]; then
    jq -cn --argjson t "$tj" --arg rid "$RID" '
      {name:"torch",
       entry:{kind:"external", tag:("pytorch-" + $t.ver),
              rids:{($rid): {archive: ($t.url|split("/")|last), url:$t.url,
                             sha256:"", size:0, extractRoot:$t.libDir}}},
       files: [$t.files[] | {name, group:"torch", sha256, size}]}' "$tj" > "$work/torch.json"
    groups_json="$(jq -c --arg g "torch" --slurpfile t "$work/torch.json" '. + {($g): $t[0].entry}' <<< "$groups_json")"
    jq -c '.files[]' "$work/torch.json" >> "$files_jsonl"
    echo "[torch] $(jq -r '.entry.tag' "$work/torch.json") · $(jq -r '.url' <<< "$tj") · $(jq -r '.files|length' <<< "$tj") file"
  fi

  # RID object: groups + files
  jq -s --argjson groups "$groups_json" '
    { groups: $groups,
      files: (map({key:.name,
                   value:({group:.group, sha256:.sha256, size:.size}
                          + (if .url then {url:.url} else {} end))}) | from_entries) }' \
    "$files_jsonl" > "$work/rid.json"

  if [ "$failed" = "1" ]; then
    echo "manifest NON aggiornato: gruppi non indicizzati (il file resta quello precedente)" >&2
    exit 1
  fi

  # manifest: merge per GRUPPO (gli altri RID restano, e restano anche i gruppi NON ricostruiti).
  # Prima si sostituiva l'intero oggetto del RID: con un build parziale (`--groups ffi`, quello che
  # usa `tools/sh/gmb_lib.sh publish`) il RID perdeva runtime/dev/llmmotor/torch → indice mutilato
  # in silenzio (rischio reale: 534 voci cancellate pubblicando una sola lib).
  [ -f "$MANIFEST" ] || echo '{"version":"0","cacheDir":"~/.gmbasic/libs","rids":{}}' > "$MANIFEST"
  local tmpm built_json; tmpm="$(mktemp)"
  built_json="$(jq -cn --arg g "$GROUP_LIST" '$g | split(",")')"
  jq --arg rid "$RID" --arg v "$(date +%Y.%m.%d)" --argjson built "$built_json" \
     --slurpfile n "$work/rid.json" '
      .version = $v
    | .cacheDir = "~/.gmbasic/libs"
    | (.rids // {}) as $rids
    | ($rids[$rid] // {}) as $old
    | ($built | map({(.): true}) | add // {}) as $b
    | .rids = ($rids + {($rid): {
          groups: ((($old.groups // {}) + (($n[0].groups) // {}))),
          # i file dei gruppi RICOSTRUITI si rimpiazzano (una lib può essere SPARITA dalla
          # release); quelli degli altri gruppi restano invariati.
          files: ( ( (($old.files) // {}) | with_entries(select(((.value.group) // "") as $g | ($b[$g] // false) | not)) )
                   + (($n[0].files) // {}) ) } })' "$MANIFEST" > "$tmpm" && mv "$tmpm" "$MANIFEST"
  # La copia SPEDITA con l'app non si scrive qui: la build la genera da questo canonico
  # (`REPL.csproj` → `<exe>/config/libs-manifest.json`). Un solo file versionato in `shared/settings/`.
  echo "manifest: $MANIFEST (RID $RID, $(jq -r --arg r "$RID" '.rids[$r].files|length' "$MANIFEST") file totali)"

  # PROIEZIONE TSV del manifest per il RID, accanto al manifest. La legge la catena C
  # (`gmb_c99native_run.sh` → `<exe>.libs.txt` → downloader runtime): prima la produceva uno
  # script Python a ogni build (`tools/data/libs_index_tsv.py`), ora è un artefatto generato
  # QUI una volta sola. Formato identico a quello che il runtime legge:
  #   riga `# requires <gruppo>-v<n> …` (solo informativa) poi  nome<TAB>sha256<TAB>size<TAB>url
  local sidecar="${MANIFEST%/*}/libs-index-$RID.tsv"
  jq -r --arg r "$RID" '
      ((.rids[$r].groups // {}) | to_entries
        | map(select(.value.version != null) | "\(.key)-\(.value.version)") | sort) as $g
    | (if ($g|length) > 0 then "# requires " + ($g|join(" ")) else empty end),
      (.rids[$r].files // {} | to_entries | sort_by(.key)[]
        | select(.value.url != null and .value.url != "")
        | [ .key, .value.sha256, (.value.size|tostring), .value.url ] | @tsv)' \
    "$MANIFEST" > "$sidecar"
  echo "indice TSV: $sidecar ($(grep -cv '^#' "$sidecar" 2>/dev/null || echo 0) file per-file)"
}

do_push() {
  [ -f "$MANIFEST" ] || { echo "manifest assente: $MANIFEST" >&2; exit 1; }
  local clone; clone="$(mktemp -d "${TMPDIR:-/tmp}/gmbmod.XXXXXX")"
  git clone --depth 1 "https://github.com/$REPO.git" "$clone" >/dev/null 2>&1
  cp "$MANIFEST" "$clone/libs-manifest.json"
  # CREDENZIALE (2026-11-10). In locale `git push` funzionava perché esiste
  # `credential.helper=store`; su un runner NON c'è alcun helper e il job `index` moriva con
  # "fatal: could not read Username for 'https://github.com'" (run 42862585667, 6/6 build+asset
  # pubblicati e indice rosso). Se `GH_TOKEN` è nell'ambiente lo si passa a git con un helper
  # EFFIMERO valido per questo solo comando: il token non finisce negli argomenti di `git`
  # (visibili in `ps`) né in un file di credenziali su disco.
  local -a cred=()
  if [ -n "${GH_TOKEN:-}" ]; then
    cred=(-c credential.helper= \
          -c 'credential.helper=!f() { test "$1" = get && printf "username=x-access-token\npassword=%s\n" "$GH_TOKEN"; }; f')
  fi
  ( cd "$clone" \
    && git add libs-manifest.json \
    && if git diff --cached --quiet; then
         # NIENTE da pubblicare: il manifest del checkout è già quello su main. Prima `git commit`
         # usciva con "nothing to commit" (rc 1) e il job `index` diventava ROSSO pur avendo fatto
         # tutto bene (visto nel run 42866953867: osx-arm64 era già pubblicato dal run prima).
         # Un no-op è un SUCCESSO: non c'è nulla da scrivere.
         echo "manifest identico a quello pubblicato: niente da pushare"
       else
         git -c user.name="gmb-libs-publisher" -c user.email="gmbasic@maxxcom.it" \
             commit -q -m "libs-manifest: aggiornato $(jq -r '.version' libs-manifest.json) ($(jq -r '.rids|keys|join(", ")' libs-manifest.json))" \
           && git ${cred[@]+"${cred[@]}"} push -q origin HEAD:main \
           && echo "manifest pubblicato: https://github.com/$REPO/blob/main/libs-manifest.json"
       fi )
  rm -rf "$clone"
}

do_show() {
  [ -f "$MANIFEST" ] || { echo "manifest assente: $MANIFEST" >&2; exit 1; }
  jq -r --arg r "$RID" '
    "version=\(.version) cache=\(.cacheDir) rid=\($r)",
    (.rids[$r].groups // {} | to_entries[] |
      "  \(.key): \(.value.kind) tag=\(.value.tag) " +
      (.value.rids[$r].archive // "") + " " + ((.value.rids[$r].size // 0)|tostring)),
    "  file: \((.rids[$r].files // {})|length)"' "$MANIFEST"
}

case "$CMD" in
  build) do_build ;;
  push)  do_push ;;
  show)  do_show ;;
  *) sed -n '2,60p' "$0" | sed 's/^# \{0,1\}//'; exit 2 ;;
esac
