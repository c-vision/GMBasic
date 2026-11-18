#!/usr/bin/env bash
# libs_publish_dev_bulk.sh — pubblica i bundle `dev` (header + statiche, un asset per file)
# rispettando i due limiti REALI di GitHub misurati il 2026-11-11:
#   1. 1000 asset per release → un tag per RID (`dev-v2` … `dev-v7`), non un tag unico;
#   2. limite anti-abuso sulle MUTAZIONI (~1250 upload, poi 403 "API rate limit exceeded"
#      su uploads.github.com e su POST /releases, anche con quota REST libera): qui si aspetta
#      e si ritenta, senza saltare file (un upload parziale è riprendibile: gli asset già
#      presenti non vengono ricaricati).
# Dopo gli upload: manifest per i 6 RID (`--groups dev`), push su GMBasicModules, indice.
# Uso: tools/sh/libs_publish_dev_bulk.sh   (staging in /tmp/dev-<rid>)
#      tools/sh/libs_publish_dev_bulk.sh --prune-report
#          per ogni RID elenca gli asset presenti nella release che NON sono più nella sorgente
#          (cioè quello che un prune rimuoverebbe) e non fa null'altro: niente creazione della
#          release, niente upload, niente manifest, push o indice.
cd /Users/gaetano/Documents/Projects/GMBasicMonoRepo || exit 1
REPORT=0
for arg in "$@"; do
  case "$arg" in
    --prune-report) REPORT=1;;
    -h|--help) sed -n '2,17p' "$0" | sed 's/^# \{0,1\}//'; exit 0;;
    *) echo "opzione sconosciuta: $arg" >&2; exit 2;;
  esac
done
. tools/sh/libs_groups.sh
REPO=c-vision/GMBasicModules; LOG=/tmp/finish-publish.log; : > "$LOG"
say(){ echo "[$(date +%H:%M:%S)] $*" | tee -a "$LOG"; }
# path → nome asset: "/" → "__"; i segmenti che iniziano/finiscono con "_" o contengono "__"
# vengono escapati ("_" → "-q-") perché si scontrerebbero col separatore. Stessa regola di
# tools/sh/libs_publish_files_gh.sh: i nomi non ambigui restano identici.
tree_path() { local out="" seg; local IFS=/ 
  for seg in $1; do case "$seg" in _*|*_|*__*) seg="${seg//_/-q-}";; esac; if [ -n "$out" ]; then out="${out}__$seg"; else out="$seg"; fi; done
  printf '%s' "$out"; }
a_tree(){ local rel="$1" stem ext
  case "$rel" in
    *.so|*.so.*) stem="${rel%%.so*}"; ext="${rel#"$stem"}";;
    *.dylib)     stem="${rel%.dylib}"; ext=".dylib";;
    *.dll)       stem="${rel%.dll}";   ext=".dll";;
    *.a)         stem="${rel%.a}";     ext=".a";;
    *.*)         stem="${rel%.*}"; ext=".${rel##*.}";;    *)           stem="$rel"; ext="";; esac
  printf '%s-%s%s' "$(tree_path "$stem")" "$RID" "$ext"; }
# esegue un comando gh finche' non riesce (403 = limite anti-abuso: si aspetta)
until_ok(){ # $1 = descrizione, resto = comando
  local d="$1"; shift
  local out rc a
  for a in $(seq 1 60); do
    out="$("$@" </dev/null 2>&1)"; rc=$?
    [ $rc -eq 0 ] && { say "$d: ok"; return 0; }
    case "$out" in
      *"rate limit"*|*"403"*) say "$d: limite GitHub, attendo 300s (tentativo $a)" >/dev/null; sleep 300;;
      *) say "$d: ${out:0:160}"; sleep 30;;
    esac
  done
  say "$d: FALLITO"; return 1; }

for pair in "osx-arm64:dev-v2" "osx-x64:dev-v3" "linux-x64:dev-v4" "linux-arm64:dev-v5" "win-x64:dev-v6" "win-arm64:dev-v7"; do
  RID="${pair%%:*}"; TAG="${pair##*:}"; LIBS_RID="$RID"; SRC="/tmp/dev-$RID"
  say "── $RID → $TAG ──"
  # release (la creazione puo' essere bloccata dal limite)
  have=$(gh api graphql -f query='query($o:String!,$r:String!,$t:String!){repository(owner:$o,name:$r){release(tagName:$t){id}}}' \
          -f o=c-vision -f r=GMBasicModules -f t="$TAG" --jq '.data.repository.release.id // empty' </dev/null 2>/dev/null)
  # --prune-report: sola lettura. `gh release view` invece della GraphQL paginata del percorso
  # di pubblicazione (quella, su un tag assente, farebbe girare until_ok per mezz'ora).
  if [ "$REPORT" = "1" ]; then
    if [ -z "$have" ]; then say "$RID: release $TAG assente — nessun resoconto"; continue; fi
    gh release view "$TAG" -R "$REPO" --json assets --jq '.assets[].name' > /tmp/exist-$RID.txt </dev/null 2>/dev/null
    exp="/tmp/exp-$RID.txt"; : > "$exp"
    while IFS= read -r f; do
      rel="${f#"$SRC"/}"
      [ -n "$(libs_group_pick dev "$rel")" ] || continue
      a_tree "$rel"; printf '\n'
    done < <(find "$SRC" -type f | sort) | sort > "$exp"
    sort /tmp/exist-$RID.txt -o /tmp/exist-$RID.sorted.txt
    comm -13 "$exp" /tmp/exist-$RID.sorted.txt > /tmp/prune-$RID.txt
    sed 's/^/  prune-report: /' /tmp/prune-$RID.txt >&2
    say "$RID: $(grep -c . /tmp/prune-$RID.txt) asset da rimuovere in $TAG (nessuno rimosso)"
    continue
  fi

  if [ -z "$have" ]; then
    until_ok "crea $TAG" gh release create "$TAG" -R "$REPO" --title "GMBasic libs — dev ($RID)" \
      --notes "Header e statiche per compilare su $RID (un asset per file; path in include__…). Indice: libs-manifest.json."
  fi
  until_ok "elenco asset $TAG" bash -c "gh api graphql --paginate \
    -f query='query(\$o:String!,\$r:String!,\$t:String!,\$endCursor:String){repository(owner:\$o,name:\$r){release(tagName:\$t){releaseAssets(first:100,after:\$endCursor){pageInfo{hasNextPage endCursor} nodes{name}}}}}' \
    -f o=c-vision -f r=GMBasicModules -f t='$TAG' --jq '.data.repository.release.releaseAssets.nodes[].name' > /tmp/exist-$RID.txt"
  files=(); assets=()
  while IFS= read -r f; do
    rel="${f#"$SRC"/}"
    [ -n "$(libs_group_pick dev "$rel")" ] || continue
    a="$(a_tree "$rel")"
    grep -qxF "$a" "/tmp/exist-$RID.txt" 2>/dev/null && continue
    files+=("$f"); assets+=("$a")
  done < <(find "$SRC" -type f | sort)
  total=${#files[@]}; say "$RID: mancanti $total su $(wc -l < /tmp/exist-$RID.txt | tr -d ' ') presenti"
  WORK="/tmp/upl-$RID"; rm -rf "$WORK"; mkdir -p "$WORK"
  i=0; okc=0; chunk=20
  while [ $i -lt $total ]; do
    batch=(); j=0
    while [ $j -lt $chunk ] && [ $((i+j)) -lt $total ]; do
      cp -f "${files[$((i+j))]}" "$WORK/${assets[$((i+j))]}"
      batch+=("$WORK/${assets[$((i+j))]}"); j=$((j+1))
    done
    if until_ok "upload $TAG +$j @$i" gh release upload "$TAG" "${batch[@]}" -R "$REPO"; then okc=$((okc+j)); fi
    i=$((i+j)); say "$RID: $okc/$total"
  done
done
if [ "$REPORT" = "1" ]; then say "FINITO (solo resoconto: nulla creato, caricato o pubblicato)"; exit 0; fi
say "=== manifest per i 6 RID ==="
for rid in osx-arm64 osx-x64 linux-x64 linux-arm64 win-x64 win-arm64; do
  until_ok "manifest $rid" tools/sh/libs_manifest_gh.sh build --rid "$rid" --groups dev
done
until_ok "push manifest" tools/sh/libs_manifest_gh.sh push
until_ok "index publish" tools/sh/libs_index_publish.sh --publish
say "FINITO"
touch /tmp/finish-done
