#!/usr/bin/env bash
# c99_module_resolve_test.sh — O7: la risoluzione moduli deve comportarsi come
# `Executor/Orchestrator.cs` (DiscoverSourceFiles + IMPORT-driven). Nessuna finestra,
# nessun link dell'app: si compila solo il resolver (`gmb_inc_resolve.c`) con un probe.
#
# Verifica:
#  1. scansione RICORSIVA della radice del programma (sottocartelle di ogni livello);
#  2. `importFolders` di app.settings.json, anche FUORI dalla radice del programma;
#  3. `importFiles` di app.settings.json, anche fuori dalla radice;
#  4. `compiler.searchFolders` con `{project}` e glob `.../*`;
#  5. IMPORT TRANSITIVI: un modulo importato da un altro modulo entra, e la dipendenza
#     viene emessa PRIMA di chi la importa;
#  6. un modulo presente fra i candidati ma mai importato NON entra nel testo;
#  7. shadowing per NOME: un file del progetto vince su quello di una cartella globale;
#  8. le cartelle di ricerca di default includono/Controls/libs restano attive.
#
# Uso: native/gmb_c99native/tests/c99_module_resolve_test.sh   (exit 0 solo se tutto passa)
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
SRC="$ROOT/native/gmb_compiler_c/frontend/src/gmb_inc_resolve.c"
INC="$ROOT/native/gmb_compiler_c/frontend/include"
[ -f "$SRC" ] || { echo "sorgente mancante: $SRC"; exit 1; }

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
fail=0
pass() { echo "PASS $1"; }
ko()   { echo "FAIL $1"; fail=1; }

# ── probe: elenca i moduli risolti, nell'ordine di emissione ───────────────────────────
cat > "$WORK/probe.c" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include "gmb_inc_resolve.h"
int main(int argc, char** argv) {
    int n = 0;
    char** f = gmb_resolve_module_files(argv[1], &n);
    for (int i = 0; i < n; i++) printf("%s\n", f[i]);
    gmb_resolve_module_files_free(f, n);
    return 0;
}
EOF
if ! cc -std=c99 -Wall -Wextra -I"$INC" "$WORK/probe.c" "$SRC" -o "$WORK/probe" 2>"$WORK/cc.err"; then
    echo "FAIL compilazione del probe"; sed -n '1,5p' "$WORK/cc.err"; exit 1
fi
pass "probe del resolver compilato"

# ── 1-7: tutte le fonti di moduli in un solo progetto ─────────────────────────────────
P="$WORK/all"
mkdir -p "$P/proj/deep/nested" "$P/proj/chain" "$P/proj/libs/widgets" "$P/proj/include" \
         "$P/proj/orphan" "$P/shared" "$P/pinned"
cat > "$P/proj/Main.bas" <<'EOF'
IMPORT DEMO_INCLUDE
IMPORT DEMO_NESTED
IMPORT DEMO_GLOB
IMPORT DEMO_SHARED
IMPORT DEMO_PINNED
IMPORT DEMO_CHAIN

SUB Main
    PRINT "ok"
END SUB
EOF
cat > "$P/proj/app.settings.json" <<'EOF'
{
    // importFolders/importFiles stanno FUORI dalla radice del programma: solo i settings
    // possono portarli dentro.
    "importFolders": ["../shared"],
    "importFiles": ["../pinned/Pinned.bas"],
    "compiler": {
        "searchFolders": [
            "include",
            "{project}/libs/*"
        ]
    }
}
EOF
cat > "$P/proj/include/Inc.bas" <<'EOF'
NAMESPACE DEMO_INCLUDE
PUBLIC FUNCTION IncValue() AS STRING
    RETURN "inc"
END FUNCTION
EOF
cat > "$P/proj/deep/nested/Nested.bas" <<'EOF'
NAMESPACE DEMO_NESTED
PUBLIC FUNCTION NestedValue() AS STRING
    RETURN "nested"
END FUNCTION
EOF
cat > "$P/proj/libs/widgets/Glob.bas" <<'EOF'
NAMESPACE DEMO_GLOB
PUBLIC FUNCTION GlobValue() AS STRING
    RETURN "glob"
END FUNCTION
EOF
cat > "$P/proj/libs/widgets/Deep.bas" <<'EOF'
NAMESPACE DEMO_DEEP
PUBLIC FUNCTION DeepValue() AS STRING
    RETURN "deep"
END FUNCTION
EOF
cat > "$P/proj/chain/Chain.bas" <<'EOF'
IMPORT DEMO_DEEP

NAMESPACE DEMO_CHAIN
PUBLIC FUNCTION ChainValue() AS STRING
    RETURN DeepValue()
END FUNCTION
EOF
cat > "$P/proj/orphan/Orphan.bas" <<'EOF'
NAMESPACE DEMO_ORPHAN
PUBLIC FUNCTION OrphanValue() AS STRING
    RETURN "mai importato"
END FUNCTION
EOF
cat > "$P/shared/Shared.bas" <<'EOF'
NAMESPACE DEMO_SHARED
PUBLIC FUNCTION SharedValue() AS STRING
    RETURN "shared"
END FUNCTION
EOF
cat > "$P/pinned/Pinned.bas" <<'EOF'
NAMESPACE DEMO_PINNED
PUBLIC FUNCTION PinnedValue() AS STRING
    RETURN "pinned"
END FUNCTION
EOF

if "$WORK/probe" "$P/proj/Main.bas" > "$WORK/all.txt" 2>"$WORK/all.err"; then
    pass "resolver completa senza errori"
else
    ko "resolver completa senza errori"; sed -n '1,3p' "$WORK/all.err"
fi

check() {
    if grep -q "$2" "$WORK/all.txt"; then pass "$1"; else
        ko "$1"; sed -n '1,20p' "$WORK/all.txt"
    fi
}
absent() {
    if grep -q "$2" "$WORK/all.txt"; then
        ko "$1"; sed -n '1,20p' "$WORK/all.txt"
    else pass "$1"; fi
}

check "scansione ricorsiva → deep/nested/Nested.bas" "/proj/deep/nested/Nested\.bas$"
check "searchFolders 'include' → include/Inc.bas" "/proj/include/Inc\.bas$"
check "searchFolders glob '{project}/libs/*' → libs/widgets/Glob.bas" "/proj/libs/widgets/Glob\.bas$"
check "importFolders → ../shared/Shared.bas" "/shared/Shared\.bas$"
check "importFiles → ../pinned/Pinned.bas" "/pinned/Pinned\.bas$"
check "IMPORT transitivo → libs/widgets/Deep.bas" "/proj/libs/widgets/Deep\.bas$"
absent "modulo mai importato escluso → orphan/Orphan.bas" "/proj/orphan/Orphan\.bas$"

# La dipendenza deve venire PRIMA di chi la importa (post-order del grafo IMPORT).
deep_line=$(grep -n "/proj/libs/widgets/Deep\.bas$" "$WORK/all.txt" | head -1 | cut -d: -f1)
chain_line=$(grep -n "/proj/chain/Chain\.bas$" "$WORK/all.txt" | head -1 | cut -d: -f1)
if [ -n "${deep_line:-}" ] && [ -n "${chain_line:-}" ] && [ "$deep_line" -lt "$chain_line" ]; then
    pass "dipendenza emessa prima del dipendente (Deep < Chain)"
else
    ko "dipendenza emessa prima del dipendente (Deep ${deep_line:-?} < Chain ${chain_line:-?})"
fi

# ── 7: shadowing per nome fra progetto e cartella globale (path ASSOLUTO) ──────────────
S="$WORK/shadow"
mkdir -p "$S/proj" "$S/global"
cat > "$S/proj/Main.bas" <<'EOF'
IMPORT DEMO_LOCAL
IMPORT DEMO_GLOBAL

SUB Main
    PRINT LocalValue()
END SUB
EOF
cat > "$S/global/Dup.bas" <<'EOF'
NAMESPACE DEMO_GLOBAL
PUBLIC FUNCTION GlobalValue() AS STRING
    RETURN "globale (deve essere shadowato)"
END FUNCTION
EOF
cat > "$S/proj/Dup.bas" <<'EOF'
NAMESPACE DEMO_LOCAL
PUBLIC FUNCTION LocalValue() AS STRING
    RETURN "locale"
END FUNCTION
EOF
cat > "$S/proj/app.settings.json" <<EOF
{
    "compiler": {
        "searchFolders": [ "$S/global" ]
    }
}
EOF
"$WORK/probe" "$S/proj/Main.bas" > "$WORK/shadow.txt" 2>/dev/null
if grep -q "/proj/Dup\.bas$" "$WORK/shadow.txt"; then pass "shadowing: vince il file del progetto"; else
    ko "shadowing: vince il file del progetto"; sed -n '1,10p' "$WORK/shadow.txt"; fi
if grep -q "/global/Dup\.bas$" "$WORK/shadow.txt"; then
    ko "shadowing: il file globale con lo stesso NOME è escluso"; sed -n '1,10p' "$WORK/shadow.txt"
else pass "shadowing: il file globale con lo stesso NOME è escluso"; fi

# ── 8: le cartelle di ricerca di DEFAULT restano attive (include/ e Controls/) ────────
D="$WORK/default"
mkdir -p "$D/proj/include" "$D/proj/Controls"
cat > "$D/proj/Main.bas" <<'EOF'
IMPORT DEMO_INC
IMPORT DEMO_CTRL

SUB Main
    PRINT IncValue(); ControlValue()
END SUB
EOF
cat > "$D/proj/include/Inc.bas" <<'EOF'
NAMESPACE DEMO_INC
PUBLIC FUNCTION IncValue() AS STRING
    RETURN "inc"
END FUNCTION
EOF
cat > "$D/proj/Controls/Ctrl.bas" <<'EOF'
NAMESPACE DEMO_CTRL
PUBLIC FUNCTION ControlValue() AS STRING
    RETURN "ctrl"
END FUNCTION
EOF
"$WORK/probe" "$D/proj/Main.bas" > "$WORK/default.txt" 2>/dev/null
if grep -q "/include/Inc\.bas$" "$WORK/default.txt" && grep -q "/Controls/Ctrl\.bas$" "$WORK/default.txt"; then
    pass "default searchFolders: include/ e Controls/"
else
    ko "default searchFolders: include/ e Controls/"; sed -n '1,10p' "$WORK/default.txt"
fi

# ── 9: `searchFolders: []` DISATTIVA le radici globali (override per chiave) ────────
# Il modulo sta FUORI dalla radice del programma: lo si vede solo dalla cartella di ricerca.
Z="$WORK/empty"
mkdir -p "$Z/proj" "$Z/global"
cat > "$Z/proj/Main.bas" <<'EOF'
IMPORT DEMO_INC

SUB Main
    PRINT IncValue()
END SUB
EOF
cat > "$Z/global/Inc.bas" <<'EOF'
NAMESPACE DEMO_INC
PUBLIC FUNCTION IncValue() AS STRING
    RETURN "inc"
END FUNCTION
EOF
cat > "$Z/proj/app.settings.json" <<EOF
{ "compiler": { "searchFolders": [ "$Z/global" ] } }
EOF
"$WORK/probe" "$Z/proj/Main.bas" > "$WORK/one.txt" 2>/dev/null
if grep -q "/global/Inc\.bas$" "$WORK/one.txt"; then
    pass "searchFolders (path assoluto) → modulo fuori dalla radice"
else
    ko "searchFolders (path assoluto) → modulo fuori dalla radice"; sed -n '1,10p' "$WORK/one.txt"
fi
cat > "$Z/proj/app.settings.json" <<'EOF'
{ "compiler": { "searchFolders": [] } }
EOF
"$WORK/probe" "$Z/proj/Main.bas" > "$WORK/empty.txt" 2>/dev/null
if [ -s "$WORK/empty.txt" ]; then
    ko "searchFolders [] disattiva le radici globali"; sed -n '1,10p' "$WORK/empty.txt"
else pass "searchFolders [] disattiva le radici globali"; fi

echo "---------------------------------"
if [ "$fail" = "0" ]; then echo "O7 module resolve: PASS"; else echo "O7 module resolve: FAIL"; fi
exit "$fail"
