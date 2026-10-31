#!/usr/bin/env bash
# gmb_cli_project_test.sh — O7: CLI di progetto (`gmb check|build|run --project`).
#
# Verifica:
#  1. `check` su un progetto valido: rc=0 e conteggio dei sorgenti del PROGETTO;
#  2. `work/` e `bin/` esclusi dalla discovery (un modulo rotto lì non conta né avvisa);
#  3. `IMPORT` non risolto = ERRORE con file:riga (rc=2);
#  4. modulo `.bas` senza `NAMESPACE` = avviso; `--strict` lo rende errore (rc=2);
#  5. entry: due `SUB Main` = errore, `--entry` esplicita lo risolve;
#  6. `--json` è JSON valido con `ok`, `sources`, `diagnostics`;
#  7. `build` produce `bin/<nome>` e `run` lo esegue con CWD = progetto;
#  8. LIBRARY: nessun `SUB Main` → l'entry È il modulo con la direttiva, il prodotto è
#     `bin/<nome-direttiva>.dylib` con l'ABI `<lib>_<Proc>` e nessun `main`; `run` non esegue.
#
# Uso: native/gmb_compiler_c/tests/gmb_cli_project_test.sh   (exit 0 solo se tutto passa)
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
GMB="$ROOT/native/gmb_compiler_c/build/gmb"
[ -x "$GMB" ] || { echo "CLI mancante: $GMB (build: cmake --build native/gmb_compiler_c/build --target gmb_cli)"; exit 1; }

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
fail=0
pass() { echo "PASS $1"; }
ko()   { echo "FAIL $1"; fail=1; }

# ── progetto valido, con work/ e bin/ che NON devono entrare ──────────────────────────
P="$WORK/ok"
mkdir -p "$P/work" "$P/bin"
cat > "$P/Main.bas" <<'EOF'
IMPORT DEMO

SUB Main
    PRINT DemoValue()
END SUB
EOF
cat > "$P/Mod.bas" <<'EOF'
NAMESPACE DEMO

PUBLIC FUNCTION DemoValue() AS STRING
    RETURN "demo"
END FUNCTION
EOF
# fuori dalla discovery: senza NAMESPACE e con un IMPORT inesistente, non deve lamentarsi
cat > "$P/work/Rotto.bas" <<'EOF'
IMPORT NON_ESISTE

SUB Rotto
    PRINT "x"
END SUB
EOF
cat > "$P/bin/AncheRotto.bas" <<'EOF'
PRINT "x"
EOF

out=$("$GMB" check --project "$P" 2>&1); rc=$?
if [ "$rc" = "0" ]; then pass "check valido → rc=0"; else ko "check valido → rc=0 (rc=$rc)"; echo "$out" | head -5; fi
if echo "$out" | grep -q "gmb check: 2 sorgenti"; then
    pass "sorgenti del progetto = 2 (work/ e bin/ esclusi)"
else
    ko "sorgenti del progetto = 2 (work/ e bin/ esclusi)"; echo "$out" | tail -3
fi
if echo "$out" | grep -q "no-namespace\|NON_ESISTE"; then
    ko "nessun avviso dai file esclusi"; echo "$out" | head -5
else pass "nessun avviso dai file esclusi"; fi

# ── IMPORT non risolto = errore ──────────────────────────────────────────────────────
U="$WORK/unres"
mkdir -p "$U"
cat > "$U/Main.bas" <<'EOF'
IMPORT FANTASMA

SUB Main
    PRINT "x"
END SUB
EOF
out=$("$GMB" check --project "$U" 2>&1); rc=$?
if [ "$rc" = "2" ] && echo "$out" | grep -q "IMPORT FANTASMA non risolto"; then
    pass "IMPORT non risolto → errore rc=2"
else
    ko "IMPORT non risolto → errore rc=2 (rc=$rc)"; echo "$out" | head -5
fi
if echo "$out" | grep -q "Main.bas:1:"; then
    pass "l'import irrisolto cita file:riga"
else
    ko "l'import irrisolto cita file:riga"; echo "$out" | head -5
fi

# ── modulo senza NAMESPACE: avviso, poi errore con --strict ─────────────────────────
N="$WORK/nonamespace"
mkdir -p "$N"
cat > "$N/Main.bas" <<'EOF'
SUB Main
    PRINT "x"
END SUB
EOF
cat > "$N/Helper.bas" <<'EOF'
FUNCTION Aiuto() AS STRING
    RETURN "a"
END FUNCTION
EOF
out=$("$GMB" check --project "$N" 2>&1); rc=$?
if [ "$rc" = "0" ] && echo "$out" | grep -q "no-namespace\|senza NAMESPACE"; then
    pass "modulo senza NAMESPACE → avviso, rc=0"
else
    ko "modulo senza NAMESPACE → avviso, rc=0 (rc=$rc)"; echo "$out" | head -5
fi
out=$("$GMB" check --project "$N" --strict 2>&1); rc=$?
if [ "$rc" = "2" ]; then pass "--strict: l'avviso diventa errore (rc=2)"; else
    ko "--strict: l'avviso diventa errore (rc=2, ottenuto $rc)"; echo "$out" | tail -3; fi

# ── entry ambigua e --entry esplicita ───────────────────────────────────────────────
A="$WORK/ambigua"
mkdir -p "$A"
printf 'SUB Main\n    PRINT "a"\nEND SUB\n' > "$A/Uno.bas"
printf 'SUB Main\n    PRINT "b"\nEND SUB\n' > "$A/Due.bas"
out=$("$GMB" check --project "$A" 2>&1); rc=$?
if [ "$rc" = "2" ] && echo "$out" | grep -q "usa --entry"; then
    pass "entry ambigua → errore con invito a --entry"
else
    ko "entry ambigua → errore con invito a --entry (rc=$rc)"; echo "$out" | head -5
fi
out=$("$GMB" check --project "$A" --entry Due.bas 2>&1); rc=$?
if [ "$rc" = "0" ]; then pass "--entry esplicita risolve l'ambiguità"; else
    ko "--entry esplicita risolve l'ambiguità (rc=$rc)"; echo "$out" | head -5; fi

# ── --json ──────────────────────────────────────────────────────────────────────────
json=$("$GMB" check --project "$P" --json 2>/dev/null)
if printf '%s' "$json" | jq -e '.ok == true and .sources == 2 and (.diagnostics | type == "array")' >/dev/null 2>&1; then
    pass "--json: ok/sources/diagnostics validi"
else
    ko "--json: ok/sources/diagnostics validi"; printf '%s\n' "$json" | head -5
fi
json=$("$GMB" check --project "$U" --json 2>/dev/null)
if printf '%s' "$json" | jq -e '.ok == false and ([.diagnostics[].code] | index("import-unresolved"))' >/dev/null 2>&1; then
    pass "--json: errore import-unresolved"
else
    ko "--json: errore import-unresolved"; printf '%s\n' "$json" | head -5
fi

# ── build + run ─────────────────────────────────────────────────────────────────────
B="$WORK/build"
mkdir -p "$B"
cat > "$B/Main.bas" <<'EOF'
SUB Main
    PRINT "cli-ok"
END SUB
EOF
out=$("$GMB" build --project "$B" 2>&1); rc=$?
if [ "$rc" = "0" ] && [ -x "$B/bin/Main" ]; then
    pass "build → bin/Main eseguibile"
else
    ko "build → bin/Main eseguibile (rc=$rc)"; echo "$out" | head -6
fi
out=$("$GMB" run --project "$B" 2>/dev/null); rc=$?
if [ "$rc" = "0" ] && [ "$out" = "cli-ok" ]; then
    pass "run → stdout del programma, rc=0"
else
    ko "run → stdout del programma, rc=0 (rc=$rc, out='$out')"
fi

# ── LIBRARY: nessun main → .gmblib, non eseguibile ────────────────────────────────
# L'estensione e i flag di nm cambiano fra Mach-O e ELF: il CLI già sceglie .dylib/.so.
case "$(uname -s)" in
    Darwin) LIBEXT="dylib"; NMDEF="-gU" ;;
    *)      LIBEXT="so";    NMDEF="-g --defined-only" ;;
esac
L="$WORK/lib"
mkdir -p "$L"
cat > "$L/GmbLibMath.bas" <<'EOF'
LIBRARY "gmbmath"

PUBLIC FUNCTION Add(a AS INTEGER, b AS INTEGER) AS INTEGER
    Add = a + b
END FUNCTION

PUBLIC FUNCTION Greet(name AS STRING) AS STRING
    Greet = "Ciao, " + name
END FUNCTION
EOF
LIBF="$L/bin/gmbmath.$LIBEXT"
# nessun --entry: un progetto LIBRARY non ha `SUB Main`, l'entry è la direttiva
out=$("$GMB" build --project "$L" 2>&1); rc=$?
if [ "$rc" = "0" ] && [ -f "$LIBF" ]; then
    pass "LIBRARY → bin/gmbmath.$LIBEXT (nome dalla direttiva, senza --entry)"
else
    ko "LIBRARY → bin/gmbmath.$LIBEXT (rc=$rc)"; echo "$out" | head -6
fi
if [ -f "$LIBF" ] && nm $NMDEF "$LIBF" 2>/dev/null | grep -q " T _\?gmbmath_Add"; then
    pass "LIBRARY → ABI esportata (gmbmath_Add)"
else
    ko "LIBRARY → ABI esportata (gmbmath_Add)"
fi
# il contratto v1 non ha entry point: un `main` nella libreria sarebbe un doppio simbolo
# nel processo che la carica
if [ -f "$LIBF" ] && ! nm $NMDEF "$LIBF" 2>/dev/null | grep -qE " T _?main$"; then
    pass "LIBRARY → nessun main esportato"
else
    ko "LIBRARY → nessun main esportato"
fi
out=$("$GMB" run --project "$L" 2>&1); rc=$?
if [ "$rc" = "0" ] && echo "$out" | grep -q "nessun entry point"; then
    pass "run su LIBRARY → costruisce e non esegue (rc=0)"
else
    ko "run su LIBRARY → costruisce e non esegue (rc=$rc)"; echo "$out" | head -6
fi
json=$("$GMB" build --project "$L" --json 2>/dev/null)
if printf '%s' "$json" | python3 -c 'import json,sys; d=json.load(sys.stdin); assert d["library"] is True; assert d["ok"] is True' 2>/dev/null; then
    pass "--json: library=true su una LIBRARY"
else
    ko "--json: library=true su una LIBRARY"; printf '%s\n' "$json" | head -8
fi
# senza `SUB Main` e senza direttiva non si indovina nulla: l'errore nomina entrambe le strade
Z="$WORK/vuoto"
mkdir -p "$Z"
printf 'SUB Aiuto\n    PRINT "x"\nEND SUB\n' > "$Z/Aiuto.bas"
out=$("$GMB" check --project "$Z" 2>&1); rc=$?
if [ "$rc" = "2" ] && echo "$out" | grep -q "LIBRARY"; then
    pass "senza SUB Main né LIBRARY → errore che nomina entrambe"
else
    ko "senza SUB Main né LIBRARY → errore che nomina entrambe (rc=$rc)"; echo "$out" | head -4
fi

# ── build fallito: il driver riporta file:riga e il CLI esce con 3/4 ────────────────
E="$WORK/errore"
mkdir -p "$E"
cat > "$E/Main.bas" <<'EOF'
SUB Main
    PRINT Calcola()
END SUB
EOF
out=$("$GMB" build --project "$E" 2>&1); rc=$?
if [ "$rc" != "0" ] && echo "$out" | grep -qE "Main\.bas:[0-9]+"; then
    pass "errore di compilazione citato con file:riga (rc=$rc)"
else
    ko "errore di compilazione citato con file:riga (rc=$rc)"; echo "$out" | head -6
fi

echo "---------------------------------"
if [ "$fail" = "0" ]; then echo "O7 CLI progetto: PASS"; else echo "O7 CLI progetto: FAIL"; fi
exit "$fail"
