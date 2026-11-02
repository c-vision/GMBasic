#!/usr/bin/env bash
# tools/sh/check_untracked_artifacts.sh — gate: bin/obj/build/out/work e binari MAI in git.
#
# Regola (AGENTS §9.6, Knowledge/Struttura-Monorepo §8.2, richiesta utente 2026-11-02):
# le cartelle di build (`bin/`, `obj/`, `build/`, `out/`) e quelle dei dati dei sample (`work/`)
# NON devono mai essere tracciate: contengono programmi BASIC compilati, oggetti, librerie e
# output delle esecuzioni. I binari (.o/.a/.dylib/.so/.dll/.exe/.pdb/.chunk/.prg/.wasm) non
# stanno in git: si distribuiscono via release/cache e si ricostruiscono.
#
# Uso: tools/sh/check_untracked_artifacts.sh [--quiet]
# Exit: 0 pulito · 1 violazioni · 2 setup (non è un repo git)
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
quiet=0
[ "${1:-}" = "--quiet" ] && quiet=1

git -C "$root" rev-parse --is-inside-work-tree >/dev/null 2>&1 || { echo "?non è un repo git: $root" >&2; exit 2; }

viol=0
report() { echo "  $*"; viol=$((viol+1)); }

# 1) nessun file TRACCIATO sotto le cartelle di artefatti
while IFS= read -r f; do
    report "tracciato sotto cartella artefatti: $f"
done < <(git -C "$root" ls-files | grep -E '(^|/)(bin|obj|build|out|work)/')

# 2) nessun binario tracciato
while IFS= read -r f; do
    report "binario tracciato: $f"
done < <(git -C "$root" ls-files | grep -E '\.(o|a|dylib|so|dll|exe|pdb|chunk|prg|wasm|gcda|gcno)$')

# 3) i pattern di ignore devono coprire davvero (check su path sintetici, --no-index)
for probe in "shared/samples/XYZ/bin/prog" "shared/samples/XYZ/obj/x" \
             "GMBasic/native/foo/build/x" "tools/out/x" "shared/samples/XYZ/work/x"; do
    if ! git -C "$root" check-ignore --no-index -q "$probe"; then
        report "path NON ignorato (pattern mancante): $probe"
    fi
done

if [ "$viol" -eq 0 ]; then
    [ "$quiet" = 1 ] || echo "UNTRACKED ARTIFACTS OK (bin/obj/build/out/work e binari mai in git)"
    exit 0
fi
echo "UNTRACKED ARTIFACTS: $viol violazioni (bin/obj/build/out/work e binari non vanno in git)"
exit 1
