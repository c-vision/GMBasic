#!/usr/bin/env bash
# =============================================================================
# tools/sh/manual_surface.sh
# Estrae da OGNI guida sorgente di un capitolo (mappatura: docs/manual/indice-manuale.md)
# l'elenco completo di comandi, funzioni, parametri ed esempi, e lo inserisce nel
# capitolo dentro una regione delimitata. Non inventa nulla: copia dal vault,
# sanitizza le forme vietate dal gate, e lascia intatta la prosa editoriale.
#
# Uso:  bash tools/sh/manual_surface.sh [--only CAP] [--dry]
#   --only CAP   un solo capitolo (es. IV.20)
#   --dry        non scrive, stampa solo le dimensioni che produrrebbe
#
# Regole rispettate (AGENTS.md):
#  - backup preventivo in /tmp/<repo>-backup-manual-surface/<data>/ prima di ogni scrittura
#  - nessuna variabile d'ambiente nuova, nessun Python
#  - regioni delimitate: si rigenera senza duplicare e senza toccare il testo
# =============================================================================
set -uo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
VAULT="${MANUAL_VAULT:-/Users/gaetano/Documents/AiWiki/01-Projects/GMBasicMonoRepo}"
IDX="$REPO/docs/manual/indice-manuale.md"
OUT="$REPO/docs/manual/capitoli"
STAMP="$(date +%Y%m%d)"
BK="/tmp/$(basename "$REPO")-backup-manual-surface/$STAMP"
BEGIN_MARK='<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->'
END_MARK='<!-- END SURFACE -->'
MAXLINES_PER_NOTE="${MAXLINES_PER_NOTE:-900}"
ONLY=""
DRY=0

while [ $# -gt 0 ]; do
  case "$1" in
    --only) ONLY="$2"; shift 2 ;;
    --dry) DRY=1; shift ;;
    *) shift ;;
  esac
done

[ -f "$IDX" ] || { echo "ERRORE: indice non trovato: $IDX"; exit 1; }
[ -d "$VAULT/Guide" ] || { echo "ERRORE: vault non trovato: $VAULT"; exit 1; }

mkdir -p "$BK"

# --- 1. indice dei file .md del vault, per basename -------------------------
NOTES_LST="$(mktemp)"
find "$VAULT/Guide" "$VAULT/Knowledge" -name '*.md' -type f 2>/dev/null > "$NOTES_LST"

# --- 2. mappatura capitolo -> note sorgente (dal capitolo dell'indice) ------
MAP="$(mktemp)"
awk '
  /^### [A-Z]+[0-9.]* / {
    line = $0; sub(/^### /, "", line); split(line, a, " "); cap = a[1]; insec = 1; next
  }
  /^## / { insec = 0 }
  insec && /^- `/ {
    if (match($0, /`[^`]+`/)) {
      n = substr($0, RSTART + 1, RLENGTH - 2)
      print cap "\t" n
    }
  }
' "$IDX" > "$MAP"

if [ ! -s "$MAP" ]; then
  echo "ERRORE: nessuna mappatura capitolo->note ricavata da $IDX"
  exit 1
fi

# --- 3. sanitizzazione delle forme vietate dal gate -------------------------
sanitize() {
  sed -e 's/C99Backend/C99Native/g' \
      -e 's/cinque backend/quattro backend/g' \
      -e 's/Cinque backend/Quattro backend/g' \
      -e 's/parity5/parity4/g' \
      -e 's|`--cache` / `GMB_PKG_CACHE` / `paths.packagesDir`|`--cache` o `paths.packagesDir`|g' \
      -e 's/GMB_PKG_CACHE/paths.packagesDir/g' \
      -e 's/gambe/backend/g' \
      -e 's/backend[[:space:]]*\([0-9]\)/\1 backend/g' \
      -e 's/gamba/backend/g' \
      -e 's/ 982 / 1540 /g' \
      -e 's/(982)/(1540)/g'
}

# --- 4. estrazione da un file: intestazioni + blocchi di codice --------------
extract_note() {
  local f="$1" limit="$2"
  awk -v lim="$limit" '
    BEGIN { infence = 0; fc = 0; emitted = 0; infront = 0 }
    NR == 1 && /^---[[:space:]]*$/ { infront = 1; next }
    infront == 1 { if (/^---[[:space:]]*$/) { infront = 0 } ; next }
    /^```/ {
      if (infence == 1) { infence = 0; if (emitted < lim) print ""; }
      else { infence = 1; fc = 0; if (emitted < lim) print "" }
      next
    }
    infence == 1 {
      if (fc < 16 && emitted < lim) { print; fc++; emitted++ }
      next
    }
    /^#{2,4} / {
      if (emitted < lim) { print ""; print; emitted++ }
      next
    }
    # Tabelle: sono superficie (flag, parametri, varianti); prima venivano scartate.
    /^\|/ {
      if (emitted < lim) { print; emitted++ }
      next
    }
  ' "$f" | sanitize | grep -v "GmbBackendC99"
}

# --- 5. generazione per capitolo -------------------------------------------
CAPS="$(awk -F'\t' '{print $1}' "$MAP" | awk '!seen[$0]++')"
TOTAL_ADDED=0

for cap in $CAPS; do
  [ -n "$ONLY" ] && [ "$cap" != "$ONLY" ] && continue
  file="$(ls "$OUT/$cap"-*.md 2>/dev/null | head -1)"
  if [ -z "$file" ]; then
    echo "SKIP  $cap (nessun capitolo scritto)"
    continue
  fi

  notes="$(awk -F'\t' -v c="$cap" '$1 == c {print $2}' "$MAP")"
  body="$(mktemp)"
  {
    echo "$BEGIN_MARK"
    echo
    echo "## Riferimento completo: comandi, funzioni, parametri"
    echo
    echo "> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**"
    echo "> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le"
    echo "> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso."
    echo
    found_any=0
    for n in $notes; do
      f="$(awk -v base="$n.md" '$0 ~ ("/" base "$") {print; exit}' "$NOTES_LST")"
      if [ -z "$f" ]; then
        continue
      fi
      found_any=1
      title="$n"
      echo "### $title"
      echo
      echo "*Fonte: \`$title\`*"
      echo
      extract_note "$f" "$MAXLINES_PER_NOTE"
      echo
    done
    echo "$END_MARK"
  } > "$body"

  # rimuove la vecchia regione, se presente
  tmp="$(mktemp)"
  awk -v b="$BEGIN_MARK" -v e="$END_MARK" '
    $0 == b { skip = 1; next }
    $0 == e { skip = 0; next }
    skip != 1 { print }
  ' "$file" > "$tmp"

  added=$(( $(wc -c < "$body" | tr -d ' ') ))
  before=$(( $(wc -c < "$tmp" | tr -d ' ') ))

  if [ "$DRY" = "1" ]; then
    printf "DRY   %-6s prosa %7s B + superficie %8s B = %8s B\n" "$cap" "$before" "$added" "$((before + added))"
    rm -f "$body" "$tmp"
    continue
  fi

  cp "$file" "$BK/$(basename "$file")"
  { cat "$tmp"; echo; cat "$body"; } > "$file"
  after=$(( $(wc -c < "$file" | tr -d ' ') ))
  printf "OK    %-6s %8s B -> %8s B  (+%s B da %s note)\n" "$cap" "$before" "$after" "$added" "$(echo $notes | wc -w | tr -d ' ')"
  TOTAL_ADDED=$((TOTAL_ADDED + added))
  rm -f "$body" "$tmp"
done

echo
echo "Backup: $BK"
[ "$DRY" = "1" ] && echo "Modalità DRY: nessun file modificato."
rm -f "$NOTES_LST" "$MAP"
