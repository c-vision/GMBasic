#!/usr/bin/env bash
# tools/sh/c99_emit_gate.sh — GATE DI EMISSIONE del backend C99Native su TUTTO il corpus.
#
# PERCHÉ. Il C99 è la base dei test (§direttiva utente 2026-11-19): prima si emette e si
# esegue sul C, l'IL/PCode/VM-C vengono dopo. Prima di questo gate nessuno sapeva quanti
# `.bas` il backend C non riuscisse nemmeno a EMETTERE: un sample che non emette non è
# "non testato", è rotto, e finché il suo unico test compila su IL il buco resta invisibile
# (caso reale: PackageServer.bas, 1800 righe, 7 errori di emissione, test solo su IL).
#
# COSA FA. Esegue il driver C (`gmbnative <file> <out.c> --no-exe`) su ogni file e registra
# l'esito. NON esegue il programma (nessun rischio di hang/server): misura solo la parte
# "parse + link + emissione C", che è il requisito minimo perché il C sia la base.
#
# USO
#   tools/sh/c99_emit_gate.sh                 # minitest + sample
#   tools/sh/c99_emit_gate.sh --minitests     # solo shared/minitests/**
#   tools/sh/c99_emit_gate.sh --samples       # solo shared/samples/**
#   tools/sh/c99_emit_gate.sh --jobs 8        # parallelismo (default: CPU/2, max 8)
#   tools/sh/c99_emit_gate.sh --timeout 30    # secondi per file (default 20)
#   tools/sh/c99_emit_gate.sh --verbose       # stampa anche gli OK
#
# USCITA. Codice 0 se TUTTO emette, 1 altrimenti (utilizzabile in CI).
# Report: tools/reports/c99_emit_gate.tsv (path, esito, prima riga d'errore) + riepilogo
# con i gruppi di causa più frequenti (i fix si fanno per causa, non per file).
set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT" || exit 2

DRV="$ROOT/GMBasic/native/gmb_compiler_c/build/gmbnative"
[ -x "$DRV" ] || DRV="$ROOT/bin/gmbnative"
if [ ! -x "$DRV" ]; then
  echo "driver C assente (atteso GMBasic/native/gmb_compiler_c/build/gmbnative)." >&2
  echo "Compila: cmake --build GMBasic/native/gmb_compiler_c/build --target gmbnative -j 6" >&2
  exit 2
fi

WANT_MINI=0; WANT_SAMPLES=0; VERBOSE=0; TIMEOUT=20
JOBS=$(( $(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4) / 2 ))
[ "$JOBS" -lt 1 ] && JOBS=1
[ "$JOBS" -gt 8 ] && JOBS=8
ONLY_PREFIX=""

while [ $# -gt 0 ]; do
  case "$1" in
    --minitests) WANT_MINI=1; shift ;;
    --samples)   WANT_SAMPLES=1; shift ;;
    --jobs)      JOBS="$2"; shift 2 ;;
    --timeout)   TIMEOUT="$2"; shift 2 ;;
    --only)      ONLY_PREFIX="$2"; shift 2 ;;   # filtro substring sul path
    --verbose)   VERBOSE=1; shift ;;
    -h|--help)   sed -n '2,30p' "$0"; exit 0 ;;
    *) echo "opzione sconosciuta: $1" >&2; exit 2 ;;
  esac
done
[ "$WANT_MINI" -eq 0 ] && [ "$WANT_SAMPLES" -eq 0 ] && { WANT_MINI=1; WANT_SAMPLES=1; }

REPORT_DIR="$ROOT/tools/reports"
mkdir -p "$REPORT_DIR"
REPORT="$REPORT_DIR/c99_emit_gate.tsv"
TMPD="$(mktemp -d "${TMPDIR:-/tmp}/c99gate.XXXXXX")" || exit 2
trap 'rm -rf "$TMPD"' EXIT

LIST="$TMPD/files.txt"
: > "$LIST"
if [ "$WANT_MINI" -eq 1 ]; then
  find shared/minitests -type f -name '*.bas' -not -path '*/bin/*' -not -path '*/work/*' 2>/dev/null >> "$LIST"
fi
if [ "$WANT_SAMPLES" -eq 1 ]; then
  find shared/samples -type f -name '*.bas' -not -path '*/bin/*' -not -path '*/work/*' 2>/dev/null >> "$LIST"
fi
if [ -n "$ONLY_PREFIX" ]; then
  grep -F "$ONLY_PREFIX" "$LIST" > "$TMPD/f2" || true
  mv "$TMPD/f2" "$LIST"
fi
sort -u "$LIST" -o "$LIST"
TOTAL=$(wc -l < "$LIST" | tr -d ' ')
if [ "$TOTAL" -eq 0 ]; then echo "nessun file da verificare" >&2; exit 2; fi

echo "gate emissione C99: $TOTAL file, jobs=$JOBS, timeout=${TIMEOUT}s" >&2

# Un job per file; ogni esito in un frammento separato (niente interleaving).
one() {
  local f="$1" tag rc err
  tag="$(printf '%s' "$f" | tr '/ ' '__')"
  err="$TMPD/e_$tag.txt"
  ( cd "$(dirname "$f")" && timeout "$TIMEOUT" "$DRV" "$ROOT/$f" "$TMPD/o_$tag.c" --no-exe ) \
      > /dev/null 2> "$err"
  rc=$?
  if [ "$rc" -eq 0 ]; then
    printf '%s\tOK\t\n' "$f" > "$TMPD/r_$tag.tsv"
  else
    local first
    first="$(grep -m1 -E 'parse error|emissione:|driver fallito|error' "$err" 2>/dev/null | tr '\t' ' ')"
    [ -z "$first" ] && first="(rc=$rc, nessun messaggio)"
    printf '%s\tFAIL\t%s\n' "$f" "$first" > "$TMPD/r_$tag.tsv"
  fi
  rm -f "$TMPD/o_$tag.c"
}
export -f one 2>/dev/null || true
export ROOT DRV TMPD TIMEOUT

# Negativo dichiarato: `' EXPECT: COMPILE ERROR: …` — il file DEVE fallire. Il gate lo tratta
# come esito atteso (colonna NEG), e segnala come FAIL un negativo che emette.
DECL="$TMPD/neg.txt"
grep -Fl "EXPECT: COMPILE ERROR" $(cat "$LIST") 2>/dev/null | sort -u > "$DECL" || true

# xargs con bash -c: la funzione non è esportabile su tutti gli shell → script inline.
cat "$LIST" | xargs -P "$JOBS" -n 1 bash -c '
  f="$1"; tag="$(printf "%s" "$f" | tr "/ " "__")"
  neg=0; grep -qxF "$f" "'"$DECL"'" 2>/dev/null && neg=1
  err="'"$TMPD"'/e_$tag.txt"
  ( cd "$(dirname "$f")" && timeout '"$TIMEOUT"' "'"$DRV"'" "'"$ROOT"'/$f" "'"$TMPD"'/o_$tag.c" --no-exe ) >/dev/null 2>"$err"
  rc=$?
  if [ "$neg" -eq 1 ]; then
    if [ "$rc" -ne 0 ]; then
      first="$(grep -m1 -E "parse error|emissione:|driver fallito|error" "$err" 2>/dev/null | tr "\t" " ")"
      printf "%s\tNEG\t%s\n" "$f" "$first" > "'"$TMPD"'/r_$tag.tsv"
    else
      printf "%s\tFAIL\tnegativo dichiarato ma EMETTE (atteso errore di compilazione)\n" "$f" > "'"$TMPD"'/r_$tag.tsv"
    fi
  elif [ "$rc" -eq 0 ]; then
    printf "%s\tOK\t\n" "$f" > "'"$TMPD"'/r_$tag.tsv"
  else
    first="$(grep -m1 -E "parse error|emissione:|driver fallito|error" "$err" 2>/dev/null | tr "\t" " ")"
    [ -z "$first" ] && first="(rc=$rc, nessun messaggio)"
    printf "%s\tFAIL\t%s\n" "$f" "$first" > "'"$TMPD"'/r_$tag.tsv"
  fi
  rm -f "'"$TMPD"'/o_$tag.c"
' _ 

cat "$TMPD"/r_*.tsv 2>/dev/null | sort > "$REPORT"

OKN=$(grep -c $'\tOK\t' "$REPORT" 2>/dev/null || true); OKN=${OKN:-0}
NEGN=$(grep -c $'\tNEG\t' "$REPORT" 2>/dev/null || true); NEGN=${NEGN:-0}
FAILN=$(grep -c $'\tFAIL\t' "$REPORT" 2>/dev/null || true); FAILN=${FAILN:-0}
echo "" >&2
echo "== gate emissione C99 ==" >&2
echo "  OK   : $OKN / $TOTAL   (emettono)" >&2
echo "  NEG  : $NEGN / $TOTAL   (falliscono come dichiarato: EXPECT: COMPILE ERROR)" >&2
echo "  FAIL : $FAILN / $TOTAL" >&2
echo "  report: $REPORT" >&2

if [ "$FAILN" -gt 0 ]; then
  echo "" >&2
  echo "-- cause più frequenti (normalizzate) --" >&2
  awk -F'\t' '$2=="FAIL" {
      e=$3;
      gsub(/[^ ]*\.bas:[0-9]+:[0-9]+: */,"",e);
      sub(/^\(rc=[0-9]+, nessun messaggio\)$/,"(nessun messaggio)",e);
      print e
    }' "$REPORT" | sort | uniq -c | sort -rn | head -15 | sed 's/^/  /' >&2
  if [ "$VERBOSE" -eq 1 ]; then
    echo "" >&2
    grep $'\tFAIL\t' "$REPORT" | sed 's/^/  /' >&2
  fi
  exit 1
fi
[ "$VERBOSE" -eq 1 ] && cat "$REPORT" >&2
exit 0
