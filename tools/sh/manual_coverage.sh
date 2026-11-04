#!/usr/bin/env bash
# manual_coverage.sh — copertura della superficie CANONICA da parte del manuale.
# Libro = capitoli EDITORIALI (A1/A2 esclusi: altrimenti si autocitano).
set -uo pipefail
REPO="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$REPO"
VAULT="${MANUAL_VAULT:-/Users/gaetano/Documents/AiWiki/01-Projects/GMBasicMonoRepo}"
R=shared/spec/registry/canonical.registry.json
BOOK=docs/manual/capitoli
TSV=docs/manual/copertura.tsv
A1=$BOOK/A1-reference-funzioni.md
A2=$BOOK/A2-costanti-tipi.md
T=$(mktemp -d)

jq -r '.procedures[] | [.ns, .name, (.arityMin//0), (.arityMax//0), (if .fn then "funzione" else "comando-qualificato" end)] | @tsv' "$R" > "$T/procs.tsv"
jq -r '.commands[] | [.name] | @tsv' "$R" > "$T/cmds.tsv"
jq -r '.constants[] | [.ns, .name, (.value|tostring)] | @tsv' "$R" > "$T/consts.tsv"
awk -F'\t' '{print toupper($1 "." $2)}' "$T/procs.tsv" > "$T/sym.txt"

# NB: shared/samples/Fixtures contiene tabelle generate dal manuale stesso (autocitazione): esclusa.
grep -rniE --exclude-dir=Fixtures --include='*.bas' --include='*.inc' '[A-Za-z][A-Za-z0-9_]*(\.[A-Za-z][A-Za-z0-9_]*)+' \
  shared/minitests shared/samples shared/include shared/libs 2>/dev/null > "$T/corpus_raw.txt"
awk -F: -v symf="$T/sym.txt" 'BEGIN{while((getline l<symf)>0) S[l]=1}
  { code=$0; sub(/^[^:]*:[^:]*:/,"",code); f=$1; ln=$2; s=code
    while (match(s,/[A-Za-z][A-Za-z0-9_]*(\.[A-Za-z][A-Za-z0-9_]*)+/)) {
      t=toupper(substr(s,RSTART,RLENGTH))
      if ((t in S) && !(t in seen)) { seen[t]=1; print t "\t" f ":" ln }
      s=substr(s,RSTART+RLENGTH) } }' "$T/corpus_raw.txt" > "$T/corpus.tsv"
if [ -f docs/manual/extra_corpus.tsv ]; then cat docs/manual/extra_corpus.tsv >> "$T/corpus.tsv"; fi

# --- chiamate a NOME NUDO: NAME(...) senza namespace (funzioni importate) ----
# Fonte dedicata: QUALUNQUE riga con una chiamata, non solo le righe che contengono
# gia un token qualificato (altrimenti ASC(), MID(), WINDOWCLOSED() restano invisibili).
grep -rniE --exclude-dir=Fixtures --include='*.bas' --include='*.inc' '[A-Za-z][A-Za-z0-9_]*[ \t]*\(' \
  shared/minitests shared/samples shared/include shared/libs 2>/dev/null > "$T/bare_raw.txt"
awk -F: '{ code=$0; sub(/^[^:]*:[^:]*:/,"",code); f=$1; ln=$2; s=code
  while (match(s, /[A-Za-z][A-Za-z0-9_]*[ \t]*\(/)) {
    t=toupper(substr(s,RSTART,RLENGTH)); sub(/[ \t]*\($/, "", t)
    if (!(t in seen)) { seen[t]=1; print t "\t" f ":" ln }
    s=substr(s,RSTART+RLENGTH) } }' "$T/bare_raw.txt" > "$T/bare.tsv"

awk '/^### [A-Z]+[0-9.]* /{l=$0; sub(/^### /,"",l); split(l,a," "); cap=a[1]; ins=1; next} /^## /{ins=0}
     ins && /^- `/ { if (match($0,/`[^`]+`/)) print substr($0,RSTART+1,RLENGTH-2) }' \
  docs/manual/indice-manuale.md | sort -u > "$T/notes.txt"
: > "$T/guides_files.txt"
while read -r n; do
  [ -z "$n" ] && continue
  f=$(find "$VAULT/Guide" "$VAULT/Knowledge" -name "$n.md" -type f 2>/dev/null | head -1)
  [ -n "$f" ] && echo "$f" >> "$T/guides_files.txt"
done < "$T/notes.txt"
if [ -s "$T/guides_files.txt" ]; then
  tr '\n' '\0' < "$T/guides_files.txt" | xargs -0 grep -hniE '[A-Za-z][A-Za-z0-9_]*(\.[A-Za-z][A-Za-z0-9_]*)+' 2>/dev/null > "$T/guides_raw.txt"
  awk -v symf="$T/sym.txt" 'BEGIN{while((getline l<symf)>0) S[l]=1}
    { s=$0
      while (match(s,/[A-Za-z][A-Za-z0-9_]*(\.[A-Za-z][A-Za-z0-9_]*)+/)) {
        t=toupper(substr(s,RSTART,RLENGTH))
        if ((t in S) && !(t in seen)) { seen[t]=1; print t "\t" $0 }
        s=substr(s,RSTART+RLENGTH) } }' "$T/guides_raw.txt" > "$T/guides.tsv"
else
  : > "$T/guides.tsv"
fi

ls "$BOOK"/*.md | grep -v -e '/A1-' -e '/A2-' > "$T/edlist.txt"
cat $(cat "$T/edlist.txt") > "$T/book_all.txt"
grep -hoiE '[A-Za-z][A-Za-z0-9_]*(\.[A-Za-z][A-Za-z0-9_]*)+' "$T/book_all.txt" | tr '[:lower:]' '[:upper:]' | sort -u > "$T/btok.txt"
awk 'BEGIN{inf=0} /^```/{inf=!inf; next} inf{ s=$0
  while (match(s,/[A-Za-z][A-Za-z0-9_]*(\.[A-Za-z][A-Za-z0-9_]*)+/)) { print toupper(substr(s,RSTART,RLENGTH)); s=substr(s,RSTART+RLENGTH) } }' \
  $(cat "$T/edlist.txt") | sort -u > "$T/bcode.txt"

awk -F'\t' -v corpus="$T/corpus.tsv" -v bare="$T/bare.tsv" -v guides="$T/guides.tsv" -v btok="$T/btok.txt" \
    -v bcode="$T/bcode.txt" -v a1="$A1" -v tsv="$TSV" \
    -f tools/sh/manual_coverage.awk "$T/procs.tsv" 2> "$T/summary.txt"
cat "$T/summary.txt"

ncmd=0; nconst=0
while IFS=$'\t' read -r cname; do
  [ -z "$cname" ] && continue
  ncmd=$((ncmd+1))
  hit=$(grep -ciE "(^|[^A-Za-z0-9_])$cname([^A-Za-z0-9_]|$)" "$T/book_all.txt")
  st="no"; [ "$hit" -gt 0 ] && st="si"
  printf "%s\tcomando\t-\t%s\t%s\t-\n" "$cname" "$st" "$st" >> "$TSV"
  printf '#### `%s` (comando)\n\n' "$cname" >> "$A1"
done < "$T/cmds.tsv"

# indice dei PRIMI usi reali delle costanti nel corpus (per l'esempio di ogni costante)
grep -rniE --exclude-dir=Fixtures --include='*.bas' --include='*.inc' '%[A-Za-z][A-Za-z0-9_]*' \
  shared/samples shared/minitests 2>/dev/null > "$T/const_raw.txt"
awk -F: '{ code=$0; sub(/^[^:]*:[^:]*:/,"",code); f=$1; ln=$2; s=code
  while (match(s, /%[A-Za-z][A-Za-z0-9_]*/)) {
    t=toupper(substr(s,RSTART,RLENGTH))
    if (!(t in seen)) { seen[t]=1; sub(/^[ \t]+/,"",code); gsub(/\t/," ",code); print t "\t" f ":" ln "\t" code }
    s=substr(s,RSTART+RLENGTH) } }' "$T/const_raw.txt" > "$T/const_use.tsv"

nsyn=0
{ echo "# A2 — Costanti globali e tipi"; echo
  echo "> Generata da \`tools/sh/manual_coverage.sh\`. Costanti dal registry canonico; per ognuna il valore,\n> un uso reale preso dal corpus (file:riga) oppure, se il corpus non la usa, un esempio\n> sintetico dichiarato con il valore canonico."
  echo; echo "## Indice delle costanti"; echo; echo "| Costante | Valore |"; echo "|---|---|"; } > "$A2"
while IFS=$'\t' read -r ns cname cval; do
  [ -z "$cname" ] && continue
  printf "| \`%s\` | %s |\n" "$cname" "$cval" >> "$A2"
done < "$T/consts.tsv"
{ echo; echo "## Costanti: valore ed esempio"; echo; } >> "$A2"
while IFS=$'\t' read -r ns cname cval; do
  [ -z "$cname" ] && continue
  nconst=$((nconst+1))
  st="no"; grep -qF "$cname" "$T/book_all.txt" && st="si"
  printf "%s\tcostante\t-\t%s\t-\t-\n" "$cname" "$st" >> "$TSV"
  ex=$(awk -F'\t' -v k="$cname" '$1==toupper(k){print $2 "\t" $3; exit}' "$T/const_use.tsv")
  printf '### `%s` — valore `%s`\n\n' "$cname" "$cval" >> "$A2"
  if [ -n "$ex" ]; then
    printf '*uso reale in `%s`*\n\n```basic\n%s\n```\n\n' "${ex%%$'\t'*}" "${ex#*$'\t'}" >> "$A2"
  else
    nsyn=$((nsyn+1))
    printf '*nessun uso nel corpus: esempio sintetico dichiarato, valore canonico `%s`*\n\n```basic\nPRINT %s\n```\n\n' "$cval" "$cname" >> "$A2"
  fi
done < "$T/consts.tsv"

grep -rhE '^[[:space:]]*TYPE[[:space:]]+[A-Za-z_]' shared/minitests shared/samples 2>/dev/null | sed -E 's/^[[:space:]]*TYPE[[:space:]]+([A-Za-z_][A-Za-z0-9_]*).*/\1/' | sort -u > "$T/types.txt"
tcount=0; tdoc=0
{ echo; echo "## Tipi e record"; echo; } >> "$A2"
while read -r tn; do
  [ -z "$tn" ] && continue
  tcount=$((tcount+1))
  f=$(grep -rlE "^[[:space:]]*TYPE[[:space:]]+$tn\b" shared/minitests shared/samples 2>/dev/null | head -1)
  [ -z "$f" ] && continue
  blk=$(awk -v t="$tn" 'BEGIN{p=0}
    !p && $0 ~ "^[[:space:]]*TYPE[[:space:]]+" t "([^A-Za-z0-9_]|$)" {p=1}
    p{print}
    p && /END[[:space:]]+TYPE/{exit}' "$f" | head -30)
  if [ -z "$blk" ]; then
    blk=$(grep -m1 -A12 -E "^[[:space:]]*TYPE[[:space:]]+$tn([^A-Za-z0-9_]|$)" "$f" 2>/dev/null)
  fi
  use=$(grep -m1 -nE "DIM[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]+AS[[:space:]]+$tn\b" "$f" 2>/dev/null)
  if [ -n "$blk" ]; then
    printf "### \`%s\`\n\n*definizione da \`%s\`*\n\n\`\`\`basic\n%s\n\`\`\`\n\n" "$tn" "$f" "$blk" >> "$A2"
  else
    printf "### \`%s\`\n\n*definizione da \`%s\` (blocco non estraibile: vedi il file)*\n\n" "$tn" "$f" >> "$A2"
  fi
  [ -n "$use" ] && printf '```basic\n%s\n```\n\n' "$use" >> "$A2"
  grep -qiF "$tn" "$T/book_all.txt" && tdoc=$((tdoc+1))
done < "$T/types.txt"

sed -i '' -e 's/C99Backend/C99Native/g' -e 's/gambe/backend/g' -e 's/gamba/backend/g' -e 's/parity5/parity4/g' "$A1" "$A2"
echo "COMANDI bare: $ncmd   COSTANTI: $nconst (senza uso nel corpus, esempio sintetico dichiarato: $nsyn)"
echo "TIPI/RECORD dal corpus: $tcount (gia citati nei capitoli: $tdoc)"
echo "A1: $(wc -c < "$A1" | tr -d ' ') B ($(grep -c '^#### ' "$A1") voci)   A2: $(wc -c < "$A2" | tr -d ' ') B ($(grep -c '^### ' "$A2") tipi)"
rm -rf "$T"
