#!/usr/bin/env bash
# tools/sh/check_work_hygiene.sh — gate di igiene sulle cartelle `work/` dei sample.
#
# Regola (AGENTS §11.1, Knowledge/Struttura-Monorepo §8.2):
#   `work/` = SOLO dati prodotti dall'ESECUZIONE del sample (DB, export, log, dump, screenshot).
#   - MAI programmi BASIC (.bas/.inc) né settings del programma (*settings*.json, gmb-package.json)
#   - MAI artefatti di build (.c, .h, .o, .a, .dylib, .so, .dll, .chunk, .prg, .exe, binari)
#   - MAI bundle Resources/ (vanno accanto all'eseguibile)
#   - MAI `work/` fuori da shared/samples/ (test/app/tooling scrivono in bin/)
# I compilati stanno nella cartella `bin/` locale al programma BASIC.
#
# Uso: tools/sh/check_work_hygiene.sh [--quiet]
# Exit: 0 pulito · 1 violazioni · 2 setup
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
quiet=0
[ "${1:-}" = "--quiet" ] && quiet=1

[ -d "$root/shared/samples" ] || { echo "?manca $root/shared/samples" >&2; exit 2; }

viol=0
report() { echo "  $*"; viol=$((viol+1)); }

# 1) file vietati dentro */work/*
while IFS= read -r f; do
    report "vietato in work/: ${f#$root/}"
done < <(find "$root/shared/samples" -path "*/work/*" -type f \( \
        -name "*.bas" -o -name "*.inc" \
        -o -name "gmb-package.json" \
        -o -name "*.c" -o -name "*.h" -o -name "*.o" -o -name "*.a" \
        -o -name "*.dylib" -o -name "*.so" -o -name "*.dll" -o -name "*.dylib.*" \
        -o -name "*.chunk" -o -name "*.prg" -o -name "*.pdb" -o -name "*.wasm" \
        -o -name "*.exe" \) 2>/dev/null)

# 1b) settings del PROGRAMMA (bundle con "paths"/"consoleConfiguration"): un file chiamato
#     app.settings.json può essere anche OUTPUT del sample (es. demo INI) → si guarda il contenuto.
while IFS= read -r f; do
    if grep -qE '"(consoleConfiguration|paths)"' "$f" 2>/dev/null; then
        report "settings del programma in work/: ${f#$root/}"
    fi
done < <(find "$root/shared/samples" -path "*/work/*" -type f -iname "*settings*.json" 2>/dev/null)

# 2) eseguibili/binari senza estensione (Mach-O / ELF / PE)
while IFS= read -r f; do
    t=$(file -b "$f" 2>/dev/null)
    case "$t" in
        *Mach-O*|*ELF*|*PE32*) report "binario in work/: ${f#$root/}" ;;
    esac
done < <(find "$root/shared/samples" -path "*/work/*" -type f 2>/dev/null)

# 3) bundle Resources dentro work/
while IFS= read -r d; do
    report "bundle Resources in work/: ${d#$root/}"
done < <(find "$root/shared/samples" -path "*/work/*" -type d -name Resources 2>/dev/null)

# 4) cartelle work/ fuori da shared/samples/
while IFS= read -r d; do
    report "work/ fuori da shared/samples: ${d#$root/}"
done < <(find "$root/shared" -type d -name work -not -path "*/shared/samples/*" 2>/dev/null)

if [ "$viol" -eq 0 ]; then
    [ "$quiet" = 1 ] || echo "WORK HYGIENE OK"
    exit 0
fi
echo "WORK HYGIENE: $viol violazioni (i compilati vanno in bin/ accanto al .bas; work/ = solo dati)"
exit 1
