#!/bin/bash
# check_memstats.sh — verifica la diagnostica di MEMORIA del runtime C (opzione `debug.memory`).
#
# Cosa controlla (gamba C99Native, la più veloce da costruire):
#   1. con l'opzione ACCESA  il programma stampa su stderr una riga `[MEM] string vivi ...`
#      e l'output su stdout NON cambia (la diagnostica non altera il programma);
#   2. con l'opzione SPENTA  nessuna riga `[MEM]` (costo nullo, nessun rumore).
#
# Non è un MiniTest: l'osservabile è su stderr, e il report esiste solo sulle gambe C
# (i backend managed usano il GC: `AI SET OPTION` lì è accettata e ignorata — differenza
# dichiarata nella guida del vault, Knowledge/Diagnostica-Memoria-Runtime-C.md).
#
# Uso: tools/sh/check_memstats.sh [--quiet]
# Exit: 0 = ok · 1 = guasto · 2 = setup
set -u
cd "$(dirname "$0")/../.." || exit 2
ROOT=$(pwd)
QUIET=0
for a in "$@"; do
  case "$a" in
    --quiet) QUIET=1 ;;
    *) echo "check_memstats: opzione sconosciuta $a" >&2; exit 2 ;;
  esac
done

RUN="$ROOT/tools/sh/gmb_c99native_run.sh"
[ -x "$RUN" ] || { echo "check_memstats: manca $RUN" >&2; exit 2; }

WORK=$(mktemp -d "${TMPDIR:-/tmp}/gmb_memstats_check.XXXXXX") || exit 2
trap 'rm -rf "$WORK"' EXIT

cat > "$WORK/on.bas" <<'EOF'
IMPORT SYSTEM
IMPORT AI
AI SET OPTION "debug.memory", "1"
DIM i AS LONG
DIM s AS STRING
FOR i = 1 TO 500
  s = "riga " + STR$(i)
NEXT i
PRINT "len="; LEN(s)
EOF
# ATTENZIONE: `AI SET OPTION` PERSISTE la voce in app.settings.json della cartella del
# programma (gmb_opt_set_persist). I due casi vanno quindi in DUE cartelle diverse,
# altrimenti l'OFF eredita il valore scritto dall'ON.
mkdir -p "$WORK/on" "$WORK/off"
mv "$WORK/on.bas" "$WORK/on/prog.bas"
sed -e 's/AI SET OPTION "debug.memory", "1"//' "$WORK/on/prog.bas" > "$WORK/off/prog.bas"

run_case() {
  # $1 = cartella del programma, $2 = suffisso
  ( cd "$ROOT" && bash "$RUN" "$1/prog.bas" "$2" ) > "$WORK/$2.out" 2> "$WORK/$2.err"
}

run_case "$WORK/on"  memon  || { echo "check_memstats: run (opzione ON) fallita" >&2; tail -3 "$WORK/memon.err" >&2; exit 1; }
run_case "$WORK/off" memoff || { echo "check_memstats: run (opzione OFF) fallita" >&2; tail -3 "$WORK/memoff.err" >&2; exit 1; }

fail=0
grep -q "^\[MEM\] string vivi" "$WORK/memon.err" || {
  echo "FAIL: con debug.memory=1 manca il report '[MEM] string vivi'" >&2
  fail=1
}
grep -q "^\[MEM\] list" "$WORK/memon.err" || {
  echo "FAIL: report incompleto (manca la riga 'list')" >&2
  fail=1
}
if grep -q "^\[MEM\]" "$WORK/memoff.err"; then
  echo "FAIL: con l'opzione SPENTA è comparso un report [MEM]" >&2
  fail=1
fi

# l'output del programma non deve cambiare fra le due esecuzioni
if ! diff -q "$WORK/memon.out" "$WORK/memoff.out" >/dev/null 2>&1; then
  echo "FAIL: l'output del programma cambia con l'opzione accesa" >&2
  diff "$WORK/memoff.out" "$WORK/memon.out" | head -5 >&2
  fail=1
fi

[ "$fail" -eq 0 ] || exit 1
if [ "$QUIET" -eq 0 ]; then
  echo "check_memstats: OK"
  grep "^\[MEM\]" "$WORK/memon.err" | sed 's/^/  /'
fi
exit 0
