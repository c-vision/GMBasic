#!/usr/bin/env bash
# tools/sh/repl_engine_parity.sh — PARITÀ fra i due engine della REPL unica.
#
#   tools/sh/repl_engine_parity.sh <file.bas>
#
# Esegue gli stessi comandi con `--engine=c` (gmb, compilatore C) e `--engine=csharp`
# (gmbcs, engine C# opzionale) e confronta l'output NORMALIZZATO. Non è una suite:
# è il test di parità mirato su un file (TODO §6, F4).
#
# Casi confrontati:
#   1. RUN            C: gmb dump chunk + gmb_vm_host   | C#: gmbcs run --engine pcode
#   2. RUN -c99       C: gmb run --project (C99Native)  | C#: gmbcs run --engine il
#   3. EVAL snippet   C: gmb eval                       | C#: gmbcs eval
# I dump (AST/PCODE/IL/DASM) NON si confrontano testualmente: sono formati diversi per
# costruzione; il test verifica che entrambi gli engine li producano (non vuoti).
set -u

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)

repl=${GMB_REPL:-$root/REPL/build/gmb_repl}
gmbcs=${GMB_CSHARP_CLI:-$root/GMBasicSharp/CompilerCli/bin/Debug/net10.0/gmbcs}
file=${1:-}
if [ -z "$file" ]; then
    echo "uso: tools/sh/repl_engine_parity.sh <file.bas>" >&2
    exit 2
fi
[ -f "$file" ] || { echo "?file non trovato: $file" >&2; exit 2; }
[ -x "$repl" ] || { echo "?REPL C non costruita: $repl" >&2; exit 2; }
if [ ! -x "$gmbcs" ]; then
    echo "SKIP: engine C# (gmbcs) assente -> parità non verificabile (solo ramo C)" >&2
    exit 0
fi

# Normalizzazione: via ANSI, banner, READY e spazi di coda.
norm() {
    sed -E 's/\x1b\[[0-9;?]*[A-Za-z]//g' \
    | grep -vE 'GMBASIC 1\.0\.0|MINARDI GAETANO|CONSOLE MODE|^READY\.$' \
    | sed -E 's/[[:space:]]+$//' \
    | sed -E '/^$/d'
}

declare -a CMDS
run_engine() {
    local eng=$1; shift
    printf '%s\nQUIT\n' "${CMDS[@]}" | "$repl" -s --engine="$eng" "$@" 2>&1 | norm
}

fails=0
compare() {
    local name=$1; shift
    local c_out cs_out
    c_out=$(run_engine c "$@")
    cs_out=$(run_engine csharp "$@")
    if [ "$c_out" = "$cs_out" ]; then
        echo "PASS  $name"
    else
        fails=$((fails + 1))
        echo "DIFF  $name"
        diff -u <(printf '%s\n' "$c_out") <(printf '%s\n' "$cs_out") | sed 's/^/      /'
    fi
}

echo "== parità engine: $(basename "$file") =="

CMDS=("RUN $file")
compare "RUN (vmc | pcode)"

CMDS=("RUN $file")
compare "RUN c99 (c99n | il)" --backend c99

CMDS=('EVAL PRINT 6*7')
compare "EVAL snippet"

# Dump: entrambi devono produrre output non vuoto (formati diversi: non si confrontano).
dump_ok() {
    local what=$1 out
    CMDS=("$what $file")
    out=$(run_engine csharp)
    if [ -n "$out" ]; then echo "PASS  dump $what (non vuoto)"; else echo "DIFF  dump $what (vuoto)"; fails=$((fails+1)); fi
}
dump_ok AST
dump_ok PCODE
dump_ok IL

if [ "$fails" -eq 0 ]; then
    echo "== PARITÀ OK =="
    exit 0
fi
echo "== $fails differenze =="
exit 1
