#!/usr/bin/env bash
# manual_domain_tables.sh — inietta in ogni capitolo una TAVOLA SINOTTICA del dominio:
# una riga per funzione/comando con firma canonica (registry) e primo uso reale (.bas/.inc).
# Idempotente: le regioni sono delimitate da marcatori HTML; backup in /tmp prima di scrivere.
set -uo pipefail
REPO="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$REPO"
BOOK=docs/manual/capitoli
R=shared/spec/registry/canonical.registry.json
BK=/tmp/GMBasicMonoRepo-backup-domain-tables/$(date +%Y%m%d-%H%M%S)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
mkdir -p "$BK"
SRC="shared/samples shared/include shared/libs shared/minitests"

# ---------- indici dei primi usi reali (Fixtures = autocitazione, esclusa) ----------
grep -rniE --exclude-dir=Fixtures --include='*.bas' --include='*.inc' \
  '[A-Za-z][A-Za-z0-9_]*\.[A-Za-z][A-Za-z0-9_]*' $SRC 2>/dev/null > "$T/raw_q.txt"
awk -F: '{ code=$0; sub(/^[^:]*:[^:]*:/,"",code); f=$1; ln=$2; s=code
  c0=code; sub(/^[ \t]+/,"",c0); if (substr(c0,1,1)==sprintf("%c",39)) next
  while (match(s,/[A-Za-z][A-Za-z0-9_]*\.[A-Za-z][A-Za-z0-9_]*/)) {
    t=toupper(substr(s,RSTART,RLENGTH))
    if (!(t in seen)) { seen[t]=1
      c=code; sub(/^[ \t]+/,"",c); gsub(/\t/," ",c); sub(/[ \t]+$/,"",c)
      if (length(c)>100) c=substr(c,1,100) "\342\200\246"
      print t "\t" f ":" ln "\t" c }
    s=substr(s,RSTART+RLENGTH) } }' "$T/raw_q.txt" > "$T/first_q.tsv"

grep -rniE --exclude-dir=Fixtures --include='*.bas' --include='*.inc' \
  '[A-Za-z][A-Za-z0-9_]*[ \t]*\(' $SRC 2>/dev/null > "$T/raw_b.txt"
awk -F: '{ code=$0; sub(/^[^:]*:[^:]*:/,"",code); f=$1; ln=$2; s=code
  c0=code; sub(/^[ \t]+/,"",c0); if (substr(c0,1,1)==sprintf("%c",39)) next
  while (match(s, /[A-Za-z][A-Za-z0-9_]*[ \t]*\(/)) {
    t=toupper(substr(s,RSTART,RLENGTH)); sub(/[ \t]*\($/,"",t)
    if (!(t in seen)) { seen[t]=1
      c=code; sub(/^[ \t]+/,"",c); gsub(/\t/," ",c); sub(/[ \t]+$/,"",c)
      if (length(c)>100) c=substr(c,1,100) "\342\200\246"
      print t "\t" f ":" ln "\t" c }
    s=substr(s,RSTART+RLENGTH) } }' "$T/raw_b.txt" > "$T/first_b.tsv"

# dump completo (per i comandi, che non hanno namespace)
grep -rnE --exclude-dir=Fixtures --include='*.bas' --include='*.inc' '' $SRC 2>/dev/null > "$T/raw_all.txt"

jq -r '.procedures[] | [.ns, .name, (.arityMin//0), (if .fn then "f" else "c" end)] | @tsv' "$R" > "$T/procs.tsv"
jq -r '.commands[] | [.name] | @tsv' "$R" > "$T/cmds.tsv"

chapter_for() {
  case "$1" in
    AI|AIAGENT) echo V.32-ai-provider-modelli-memoria.md ;;
    AGENT|WORKFLOW) echo V.33-agenti-workflow.md ;;
    VECTOR|RAG|EMBEDDING|NOTE|BRAIN) echo V.34-rag-vettori-second-brain.md ;;
    TRADING|SIGNAL|TIMESERIES|PORTFOLIO|BONDS|OPTIONS|METRICS) echo V.28-segnali-trading-blockchain.md ;;
    STATS|MATH|ANALYSIS|OPTIMIZE|MATRIX|GRAPH) echo V.27-matematica-statistica.md ;;
    STRINGS|ARRAYS|COLLECTIONS|DICT|LIST) echo II.9-array-collezioni-stringhe.md ;;
    CRYPTOGRAPHY|CRYPTO|HASH|SECURITY) echo IV.26-sicurezza-crittografia.md ;;
    TORCH) echo V.31-torch.md ;;
    DBF|ISAM|SPEED|RDD|LEDGER|SQL) echo IV.23-database.md ;;
    COMPRESSION|ARCHIVE|BITS|BINARY) echo V.30-compressione-binario.md ;;
    LLM|TRAIN|MODEL) echo V.35-llm-locale.md ;;
    GRAPHICS|SPRITE|CHART|POLYGON|GAME|SOUND|AUDIO) echo IV.21-grafica-suono-giochi.md ;;
    CONTROLS|CONTROL) echo IV.20-controlli-utente.md ;;
    MOUSE|WINDOWS|WINDOW|TRAY|KEY|KEYBOARD|JOYSTICK) echo IV.19-finestre-eventi.md ;;
    CONSOLE|SCREEN|TUI) echo IV.17-console-tui.md ;;
    CONVERSIONS|TYPES) echo II.5-tipi-variabili-scope.md ;;
    SPECIAL|SYSTEM|RUNTIME) echo III.12-dentro-runtime.md ;;
    HTTP|NET|WEB|WEBSOCKET|URL|CURL) echo IV.24-rete-web.md ;;
    DATETIME|DATE|TIME) echo V.29-date-ore-sistema.md ;;
    FILE|FILES|IO|DOCUMENT|DOC|PDF|CSV|JSON|XML|ZIP) echo IV.22-file-io-documenti.md ;;
    MESSAGE|QUEUE|MQTT|KAFKA|AMQP) echo IV.25-messaggistica-code.md ;;
    DEBUG|TEST|DIAG) echo III.14-debug-diagnostica.md ;;
    ATOMIC|CONCURRENCY|CONTEXT|MEMORY|MUTEX|POOL|SYNCMAP|THREAD) echo III.13-gpm-concorrenza.md ;;
    AUTH|JWT|TOKEN) echo IV.26-sicurezza-crittografia.md ;;
    BLOCKCHAIN|FINANCE|BACKTESTS|BONDS|OPTIONS|PORTFOLIO|METRICS|BETTING) echo V.28-segnali-trading-blockchain.md ;;
    BROWSER|COMPUTER|VISION) echo V.36-computer-use-browser.md ;;
    VECTORDB|KDTREE|SPATIAL|GREP|MEMORYMAP|TOOL) echo V.34-rag-vettori-second-brain.md ;;
    ASR|TTS|OCR) echo V.32-ai-provider-modelli-memoria.md ;;
    CLIP|UI) echo IV.20-controlli-utente.md ;;
    CONVERT) echo II.5-tipi-variabili-scope.md ;;
    TYPEINFO) echo III.12-dentro-runtime.md ;;
    JOY) echo IV.19-finestre-eventi.md ;;
    *) echo "" ;;
  esac
}

# indice delle COPPIE di parole adiacenti: copre la sintassi a spazio (LEDGER EDIT h ID ...)
awk '{ line=$0; f=line; sub(/:.*/,"",f); rest=line; sub(/^[^:]*:[^:]*:/,"",rest)
  c0=rest; sub(/^[ \t]+/,"",c0); if (substr(c0,1,1)==sprintf("%c",39)) next
  c=rest; sub(/^[ \t]+/,"",c); gsub(/\t/," ",c); sub(/[ \t]+$/,"",c)
  n=split(toupper(rest), w, /[^A-Z0-9_]+/)
  for(i=1;i<n;i++){ k=w[i] w[i+1]
    if (length(k)<=30 && !(k in got)) { got[k]=1; print k "\t" f ":" NR "\t" c } } }' \
  "$T/raw_all.txt" > "$T/pairs.tsv"

# indice del PRIMO token di istruzione: copre i comandi a parola singola senza parentesi (AINS a, 1)
awk '{ line=$0; f=line; sub(/:.*/,"",f); rest=line; sub(/^[^:]*:[^:]*:/,"",rest)
  c0=rest; sub(/^[ \t]+/,"",c0); if (substr(c0,1,1)==sprintf("%c",39)) next
  c=rest; sub(/^[ \t]+/,"",c); gsub(/\t/," ",c); sub(/[ \t]+$/,"",c)
  if (length(c)>100) c=substr(c,1,100) "\342\200\246"
  n=split(toupper(rest), w, /[^A-Z0-9_]+/)
  k=w[1]
  if (k!="" && length(k)<=24 && !(k in got)) { got[k]=1; print k "\t" f ":" NR "\t" c } }' \
  "$T/raw_all.txt" > "$T/first_tok.tsv"

# ---------- righe: capitolo <TAB> riga markdown ----------
: > "$T/rows.tsv"; : > "$T/unmapped.txt"
awk -F'\t' -v qf="$T/first_q.tsv" -v bf="$T/first_b.tsv" -v pf="$T/pairs.tsv" -v tf="$T/first_tok.tsv" '
  BEGIN{ while((getline l < "docs/manual/extra_examples.tsv")>0){ split(l,p,"\t"); if(!(p[1] in O)) O[p[1]]=p[2] "\t" p[3] }
         while((getline l < qf)>0){ split(l,p,"\t"); if(!(p[1] in Q)) Q[p[1]]=p[2] "\t" p[3] }
         while((getline l < bf)>0){ split(l,p,"\t"); if(!(p[1] in B)) B[p[1]]=p[2] "\t" p[3] }
         while((getline l < pf)>0){ split(l,p,"\t"); if(!(p[1] in P)) P[p[1]]=p[2] "\t" p[3] }
         while((getline l < tf)>0){ split(l,p,"\t"); if(!(p[1] in T)) T[p[1]]=p[2] "\t" p[3] } }
  { ns=$1; nm=$2; a=$3+0; sym=toupper(ns "." nm); arg=""
    for(i=0;i<a;i++) arg=arg (i?", ":"") "x" (i+1)
    ex=""
    if (sym in O) ex=O[sym]
    else if (sym in Q) ex=Q[sym]
    else if (toupper(nm) in B) ex=B[toupper(nm)]
    else { k=toupper(ns nm); gsub(/[^A-Z0-9_]/,"",k)
           if (k in P) ex=P[k]
           else if (toupper(nm) in T) ex=T[toupper(nm)] }
    line="- **" sym "(" arg ")**"
    if (ex!="") { split(ex,e,"\t"); line=line " — `" e[2] "` (" e[1] ")" }
    else line=line " — nessun uso nel corpus"
    print sym "\t" ns "\t" line }' "$T/procs.tsv" > "$T/proc_rows.tsv"

while IFS=$'\t' read -r sym ns line; do
  ch=$(chapter_for "${ns%%.*}")
  if [ -z "$ch" ]; then printf '%s\t%s\n' "$ns" "$sym" >> "$T/unmapped.txt"; ch=III.12-dentro-runtime.md; fi
  printf '%s\t%s\n' "$ch" "$line" >> "$T/rows.tsv"
done < "$T/proc_rows.tsv"

# ---------- comandi: tavola unica in II.4 (lessico delle istruzioni) ----------
awk -F'\t' -v cf="$T/cmds.tsv" -v all="$T/raw_all.txt" -v outf="$T/cmd_rows.tsv" '
  BEGIN{ while((getline l < cf)>0) if(l!="") NAME[l]=1
         while((getline l < all)>0){
           f=l; sub(/:.*/,"",f); rest=l; sub(/^[^:]*:[^:]*:/,"",rest)
           c=rest; sub(/^[ \t]+/,"",c); gsub(/\t/," ",c); sub(/[ \t]+$/,"",c)
           if (length(c)>100) c=substr(c,1,100) "\342\200\246"
           n=split(rest,w,/[^A-Za-z0-9_]+/)
           for(i=1;i<=n;i++){ t=w[i]; if((t in NAME) && !(t in got)){ got[t]=1; print t "\t" f "\t" c } } }
  }' > /dev/null
awk -F'\t' -v all="$T/raw_all.txt" 'BEGIN{
    while((getline l < "/dev/stdin")>0) {}
  }' < /dev/null

# (indice comandi calcolato in modo lineare, una sola passata)
awk -v cfile="$T/cmds.tsv" -v all="$T/raw_all.txt" -v outf="$T/cmd_rows.tsv" '
  BEGIN{ while((getline l < cfile)>0){ gsub(/[ \t]/,"",l); if(l!="") NAME[toupper(l)]=1 }
         split("",got) }
  { line=$0; f=line; sub(/:.*/,"",f); rest=line; sub(/^[^:]*:[^:]*:/,"",rest)
    c0=rest; sub(/^[ \t]+/,"",c0); if (substr(c0,1,1)==sprintf("%c",39)) next
    c=rest; sub(/^[ \t]+/,"",c); gsub(/\t/," ",c); sub(/[ \t]+$/,"",c)
    if (length(c)>100) c=substr(c,1,100) "\342\200\246"
    n=split(toupper(rest), w, /[^A-Z0-9_]+/)
    for(i=1;i<=n;i++){
      t=w[i]
      if((t in NAME) && !(t in got)){ got[t]=1; print t "\t" f "\t" c > outf }
      # comandi in due parole scritte separate nel sorgente (SAVE SCREEN, LINE INPUT, SET OPTION)
      if(i<n){ t2=w[i] w[i+1]
        if((t2 in NAME) && !(t2 in got)){ got[t2]=1; print t2 "\t" f "\t" c > outf } } } }' "$T/raw_all.txt"

awk -F'\t' '{ n=$1; k=n; gsub(/[ \t]/,"",k); if(k!="") print toupper(k) "\t" n }' "$T/cmds.tsv" > "$T/cmd_disp.tsv"
: > "$T/cmd_rows.md"
while IFS=$'\t' read -r cn f l; do
  dn=$(awk -F'\t' -v k="$cn" '$1==k{print $2; exit}' "$T/cmd_disp.tsv")
  [ -z "$dn" ] && dn="$cn"
  printf -- '- **%s** — `%s` (%s)\n' "$dn" "$l" "$f" >> "$T/cmd_rows.md"
done < "$T/cmd_rows.tsv"
ncmd_doc=$(wc -l < "$T/cmd_rows.md" | tr -d ' ')
ncmd_all=$(wc -l < "$T/cmds.tsv" | tr -d ' ')

if false; then   # post-pass disabilitato: per i casi residui si usa docs/manual/extra_examples.tsv
# ---------- ultima risorsa: righe rimaste senza esempio -> grep mirato sulla forma comando ----------
# Copre le istruzioni a parola singola senza parentesi (es. "AINS a, 1"), che nessun indice
# per token intercetta in modo affidabile.
awk -F'\t' '$2 ~ /nessun uso nel corpus/ { match($2, /\*\*[^*]+\*\*/); print substr($2, RSTART+2, RLENGTH-4) }' \
  "$T/rows.tsv" | while IFS= read -r sym; do
  [ -z "$sym" ] && continue
  sym=${sym%%(*}; nm=${sym#*.}
  hit=$(grep -m1 -rniE --exclude-dir=Fixtures --include='*.bas' --include='*.inc' \
        "^[[:space:]]*$nm([[:space:],)]|\$)" shared/samples shared/include shared/libs shared/minitests 2>/dev/null)
  [ -z "$hit" ] && continue
  hf=${hit%%:*}; hrest=${hit#*:}; hln=${hrest%%:*}; hcode=${hrest#*:}
  hcode=$(printf '%s' "$hcode" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')
  [ ${#hcode} -gt 100 ] && hcode="${hcode:0:100}…"
  newrow=$(printf -- '- **%s** — `%s` (%s:%s)' "$sym" "$hcode" "$hf" "$hln")
  awk -F'\t' -v s="$sym" -v r="$newrow" 'BEGIN{OFS="\t"}
    { if (index($2, "**" s "(")>0 && index($2, "nessun uso nel corpus")>0) $2=r; print }' \
    "$T/rows.tsv" > "$T/rows.new"
  mv "$T/rows.new" "$T/rows.tsv"
done
fi

# ---------- iniezione idempotente ----------
inject() { # $1 capitolo  $2 file con le righe
  f="$BOOK/$1"; [ -f "$f" ] || return 0
  cp "$f" "$BK/$1"
  awk 'BEGIN{s=0} /^<!-- BEGIN DOMAIN TABLE/{s=1; next} /^<!-- END DOMAIN TABLE/{s=0; next} !s{print}' "$f" > "$T/body.md"
  {
    echo
    echo "<!-- BEGIN DOMAIN TABLE -->"
    echo "## Tavola sinottica del dominio"
    echo
    printf '%s\n' 'Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato'
    printf '%s\n' 'nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.'
    echo
    cat "$2"
    echo "<!-- END DOMAIN TABLE -->"
  } > "$T/block.md"
  cat "$T/body.md" "$T/block.md" > "$f"
}

# per capitolo: righe delle sue funzioni
for f in "$BOOK"/*.md; do
  b=$(basename "$f"); case "$b" in A1-*|A2-*) continue;; esac
  awk -F'\t' -v c="$b" '$1==c{print substr($0, index($0,"\t")+1)}' "$T/rows.tsv" > "$T/one.md"
  [ -s "$T/one.md" ] && inject "$b" "$T/one.md"
done
if [ -s "$T/cmd_rows.md" ]; then
  inject II.4-grammatica-lessico.md "$T/cmd_rows.md"
fi

echo "capitoli toccati: $(ls "$BK" | wc -l | tr -d ' ')   backup: $BK"
echo "righe funzione complessive: $(wc -l < "$T/rows.tsv" | tr -d ' ')"
echo "comandi con uso reale: $ncmd_doc / $ncmd_all"
if [ -s "$T/unmapped.txt" ]; then echo "NAMESPACE NON MAPPATI (finiti in III.12):"; cut -f1 "$T/unmapped.txt" | sort -u | tr '\n' ' '; echo; fi
if [ "$ncmd_doc" -lt "$ncmd_all" ]; then
  echo "COMANDI SENZA USO NEL CORPUS:"; cut -f1 "$T/cmds.tsv" | while read -r c; do grep -qF "**$c**" "$T/cmd_rows.md" || printf '%s ' "$c"; done; echo
fi
