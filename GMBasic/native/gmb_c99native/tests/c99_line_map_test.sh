#!/usr/bin/env bash
# c99_line_map_test.sh — O8 (mappa diagnostica C→BASIC), senza finestra e senza link.
#
# Verifica:
#  1. ogni statement emette `#line <riga bas> "<file bas>"` e la direttiva è attaccata al
#     C che quello statement ha prodotto;
#  2. con più moduli IMPORTati la direttiva cita il file giusto (Ui.bas per il modulo,
#     Main.bas per il chiamante) e la riga riparte da 1 dentro ogni file;
#  3. `--line-map` scrive il JSON degli anchor (riga C → file/riga BASIC);
#  4. un errore di emissione riporta `file:riga` del `.bas`, non la riga del testo unito;
#  5. le direttive sono sempre a inizio riga (un `#line` a metà espressione romperebbe il C).
#
# Uso: native/gmb_c99native/tests/c99_line_map_test.sh   (exit 0 solo se tutte le asserzioni passano)
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
DRV="$ROOT/native/gmb_compiler_c/build/gmb_c99native_driver"
[ -x "$DRV" ] || { echo "driver mancante: $DRV (build: cmake --build native/gmb_compiler_c/build --target gmb_c99native_driver)"; exit 1; }

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
fail=0
pass() { echo "PASS $1"; }
ko()   { echo "FAIL $1"; fail=1; }

mkcase() { mkdir -p "$WORK/$1"; }

# ── 1/2/3: modulo IMPORTato + chiamante ───────────────────────────────────────────────
mkcase multi
cat > "$WORK/multi/Main.bas" <<'EOF'
IMPORT UI

SUB Main
    frmShow
    PRINT "dopo"
END SUB
EOF
cat > "$WORK/multi/Ui.bas" <<'EOF'
NAMESPACE UI

SUB frmShow
    PRINT "hello dalla form"
END SUB
EOF

if "$DRV" "$WORK/multi/Main.bas" "$WORK/multi/main.c" --no-exe --line-map "$WORK/multi/map.json" \
        >"$WORK/multi/out.log" 2>&1; then
    pass "driver emette il C (multi-file)"
else
    ko "driver emette il C (multi-file)"; sed -n '1,5p' "$WORK/multi/out.log"
fi

grep -Eq '#line 4 ".*/Ui\.bas"' "$WORK/multi/main.c" \
    && pass "direttiva del modulo → Ui.bas:4" \
    || ko "direttiva del modulo → Ui.bas:4"

grep -Eq '#line 4 ".*/Main\.bas"' "$WORK/multi/main.c" \
    && pass "direttiva del chiamante → Main.bas:4" \
    || ko "direttiva del chiamante → Main.bas:4"

# La direttiva deve precedere il C dello statement (che può occupare più righe), non
# restare appesa a un blocco dichiarativo che non emette codice.
awk '/#line 4 ".*\/Ui\.bas"/{for (i = 0; i < 4; i++) { getline; if ($0 ~ /gmb_print_string/) found = 1 }}
     END { exit found ? 0 : 1 }' "$WORK/multi/main.c" \
    && pass "la direttiva precede il C dello statement" \
    || ko "la direttiva precede il C dello statement"

# 5: ogni direttiva è ben formata e a inizio riga (un `#line` a metà espressione
# romperebbe il C).
if grep -En '^#line [0-9]+ "/' "$WORK/multi/main.c" >/dev/null \
   && ! grep -En '.[^[:space:]]#line [0-9]+ "' "$WORK/multi/main.c" >/dev/null; then
    pass "direttive ben formate e solo a inizio riga"
else
    ko "direttive ben formate e solo a inizio riga"
fi

# 3: anchor C → BASIC nel JSON.
if [ -s "$WORK/multi/map.json" ] \
   && grep -q '"c_line"' "$WORK/multi/map.json" \
   && grep -q 'Ui\.bas"' "$WORK/multi/map.json"; then
    # la riga C dell'anchor deve contenere davvero il C dello statement
    cline=$(sed -nE 's/.*"c_line": ([0-9]+), "line": 4, "file": ".*\/Ui\.bas".*/\1/p' "$WORK/multi/map.json" | head -1)
    # Lo statement PRINT occupa più righe C: l'anchor punta alla prima.
    if [ -n "${cline:-}" ] && sed -n "${cline},$((cline + 4))p" "$WORK/multi/main.c" | grep -q "gmb_print_string"; then
        pass "--line-map: c_line ${cline} → Ui.bas:4"
    else
        ko "--line-map: c_line punta al C dello statement"
    fi
else
    ko "--line-map scrive gli anchor"
fi

# ── 4: errore di emissione con file/riga BASIC ─────────────────────────────────────────
mkcase err
cat > "$WORK/err/Main.bas" <<'EOF'
IMPORT UI

SUB Main
    calcola
END SUB
EOF
cat > "$WORK/err/Ui.bas" <<'EOF'
NAMESPACE UI

SUB calcola
    DIM t AS DOUBLE
    t = TIMER()
    PRINT t
END SUB
EOF
"$DRV" "$WORK/err/Main.bas" "$WORK/err/main.c" --no-exe >"$WORK/err/out.log" 2>&1
rc=$?
if [ "$rc" = "4" ] && grep -Eq 'emissione: .*Ui\.bas:5: .*TIMER' "$WORK/err/out.log"; then
    pass "errore di emissione → Ui.bas:5 (rc=4)"
else
    ko "errore di emissione → Ui.bas:5 (rc=4, ottenuto rc=$rc)"
    sed -n '1,3p' "$WORK/err/out.log"
fi

# ── 5: nessuna direttiva appesa senza C (dichiarazioni che non emettono codice) ────────
mkcase decl
cat > "$WORK/decl/only.bas" <<'EOF'
NAMESPACE DEMO

SUB Main
    PRINT "ciao"
END SUB
EOF
"$DRV" "$WORK/decl/only.bas" "$WORK/decl/only.c" --no-exe >/dev/null 2>&1
# Ogni direttiva deve avere del C subito dopo (non un'altra direttiva).
if awk '/^#line /{ getline; while ($0 ~ /^[[:space:]]*$/) getline;
                   if ($0 ~ /^#line /) bad = 1 }
        END { exit bad ? 1 : 0 }' "$WORK/decl/only.c"; then
    pass "nessuna direttiva appesa a vuoto"
else
    ko "nessuna direttiva appesa a vuoto"
fi

echo "---------------------------------"
if [ "$fail" = "0" ]; then echo "O8 line map: PASS"; else echo "O8 line map: FAIL"; fi
exit "$fail"
