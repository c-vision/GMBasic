#!/usr/bin/env bash
# gmb_cli_minitests_test.sh — `gmb minitests` = punto UNICO di verifica della suite.
#
# Verifica, senza toccare la suite vera (usa una radice sintetica in mktemp):
#   1. scoperta RICORSIVA e ordinata dei `.bas` (anche in sottocartelle annidate);
#   2. classificazione PASS / CC-FAIL / DRIVER-FAIL e conteggi coerenti;
#   3. un driver ucciso da un segnale è un DRIVER-FAIL `crash` (rc 128+segnale), non un CC-FAIL;
#   4. TAG `interactive`/`heavy` esclusi di default, inclusi con --all;
#   5. exit code: 0 tutto PASS · 2 almeno un FAIL · 1 setup rotto (driver/radice);
#   6. --json è JSON valido, con `failures` ordinate e gli stessi numeri del testo;
#   7. il run è DETERMINISTICO (due esecuzioni ⇒ JSON identico).
#
# Uso: native/gmb_compiler_c/tests/gmb_cli_minitests_test.sh  (exit 0 solo se tutto passa)
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
GMB="$ROOT/native/gmb_compiler_c/build/gmb"
[ -x "$GMB" ] || { echo "gmb mancante: $GMB (build: cmake --build native/gmb_compiler_c/build --target gmb_cli)"; exit 1; }

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
fail=0
pass() { echo "PASS $1"; }
ko()   { echo "FAIL $1"; fail=1; }

json_get() { # json_get <file> <chiave numerica o booleana>
    jq -r ".$2" "$1"
}

# ── radici sintetiche ─────────────────────────────────────────────────────────────────────
GOOD="$WORK/good"
BAD="$WORK/bad"
mkdir -p "$GOOD/deep/nested" "$BAD/tagged"
cat > "$GOOD/Main.bas" <<'EOF'
SUB Main
    PRINT "ok"
END SUB
EOF
cat > "$GOOD/deep/nested/helper.bas" <<'EOF'
NAMESPACE DEEP

SUB aiuto
    PRINT "annidato"
END SUB
EOF
cat > "$BAD/Main.bas" <<'EOF'
SUB Main
    PRINT "ok"
END SUB
EOF
cat > "$BAD/rotto.bas" <<'EOF'
SUB Main
    IF THEN QUESTO NON E' GMB
END SUB
EOF
cat > "$BAD/tagged/interactive.bas" <<'EOF'
' TAG: interactive
SUB Main
    PRINT "interattivo"
END SUB
EOF
cat > "$BAD/tagged/heavy.bas" <<'EOF'
' TAG: heavy
SUB Main
    PRINT "pesante"
END SUB
EOF

# ── 1/5: tutti PASS, ricorsivo, exit 0 ───────────────────────────────────────────────────
"$GMB" minitests --root "$GOOD" --json > "$WORK/good.json" 2> "$WORK/good.err"
rc=$?
[ "$rc" = "0" ] && pass "tutti PASS → rc 0" || { ko "tutti PASS → rc 0 (rc=$rc)"; cat "$WORK/good.err"; }
[ "$(json_get "$WORK/good.json" total)" = "2" ] \
    && pass "scoperta ricorsiva: 2 file (uno annidato)" \
    || ko "scoperta ricorsiva: 2 file (total=$(json_get "$WORK/good.json" total))"
[ "$(json_get "$WORK/good.json" pass)" = "2" ] \
    && pass "2 PASS" || ko "2 PASS (pass=$(json_get "$WORK/good.json" pass))"
[ "$(json_get "$WORK/good.json" ok)" = "true" ] && pass "ok=true" || ko "ok=true"

# ── 2/5: un driver KO ⇒ DRIVER-FAIL, exit 2 ──────────────────────────────────────────────
"$GMB" minitests --root "$BAD" --json > "$WORK/bad.json" 2> "$WORK/bad.err"
rc=$?
[ "$rc" = "2" ] && pass "FAIL presente → rc 2" || { ko "FAIL presente → rc 2 (rc=$rc)"; cat "$WORK/bad.err"; }
[ "$(json_get "$WORK/bad.json" driver_fail)" = "1" ] \
    && pass "1 DRIVER-FAIL" || ko "1 DRIVER-FAIL (driver_fail=$(json_get "$WORK/bad.json" driver_fail))"
[ "$(json_get "$WORK/bad.json" pass)" = "1" ] \
    && pass "1 PASS accanto al FAIL" || ko "1 PASS accanto al FAIL"
grep -q 'DRIVER-FAIL .*rotto\.bas' "$WORK/bad.err" \
    && pass "il file rotto è nominato nell'output" || ko "il file rotto è nominato nell'output"
"$GMB" minitests --root "$BAD" > /dev/null 2> "$WORK/bad_human.err"
# la riga storica di tools/c99_compile_errors.sh resta nel riepilogo leggibile
# (non in --json, dove stdout è solo dati)
grep -q '^C99 compile-error: 0$' "$WORK/bad_human.err" \
    && pass 'riga storica C99 compile-error: presente' || ko 'riga storica C99 compile-error: presente'

# ── 4: TAG interactive/heavy esclusi di default, inclusi con --all ───────────────────────
[ "$(json_get "$WORK/bad.json" skipped_interactive)" = "1" ] \
    && pass "interactive escluso di default" || ko "interactive escluso di default"
[ "$(json_get "$WORK/bad.json" skipped_heavy)" = "1" ] \
    && pass "heavy escluso di default" || ko "heavy escluso di default"
"$GMB" minitests --root "$GOOD" --filter nested > /dev/null 2>&1 \
    && pass "--filter sceglie per path" || ko "--filter sceglie per path"
"$GMB" minitests --root "$BAD" --all --json > "$WORK/all.json" 2>/dev/null
[ "$(json_get "$WORK/all.json" run)" = "4" ] \
    && pass "--all esegue anche i TAG" || ko "--all esegue anche i TAG (run=$(json_get "$WORK/all.json" run))"

# ── 2b: un cc che fallisce ⇒ CC-FAIL (classificazione distinta dal driver) ───────────────
"$GMB" minitests --root "$GOOD" --cc /usr/bin/false --json > "$WORK/cc.json" 2>/dev/null
[ "$(json_get "$WORK/cc.json" cc_fail)" = "2" ] \
    && pass "cc KO → 2 CC-FAIL" || ko "cc KO → 2 CC-FAIL (cc_fail=$(json_get "$WORK/cc.json" cc_fail))"
[ "$(json_get "$WORK/cc.json" driver_fail)" = "0" ] \
    && pass "cc KO non è un DRIVER-FAIL" || ko "cc KO non è un DRIVER-FAIL"

# ── 3: crash del driver (SIGSEGV) ⇒ DRIVER-FAIL con crash:true, non CC-FAIL ──────────────
cat > "$WORK/crashdriver" <<'EOF'
#!/bin/sh
kill -SEGV $$
EOF
chmod +x "$WORK/crashdriver"
"$GMB" minitests --root "$GOOD" --driver "$WORK/crashdriver" --json > "$WORK/crash.json" 2>/dev/null
[ "$(json_get "$WORK/crash.json" driver_crash)" = "2" ] \
    && pass "segnale del driver → 2 crash" || ko "segnale del driver → 2 crash (crash=$(json_get "$WORK/crash.json" driver_crash))"
[ "$(json_get "$WORK/crash.json" cc_fail)" = "0" ] \
    && pass "il crash non viene contato come CC-FAIL" || ko "il crash non viene contato come CC-FAIL"

# ── 5: setup rotto ⇒ rc 1 (non 2: non è un verdetto sul codegen) ─────────────────────────
"$GMB" minitests --root "$GOOD" --driver "$WORK/nonesiste" > /dev/null 2>&1
[ "$?" = "1" ] && pass "driver inesistente → rc 1" || ko "driver inesistente → rc 1"
"$GMB" minitests --root "$WORK/nonesiste" > /dev/null 2>&1
[ "$?" = "1" ] && pass "radice inesistente → rc 1" || ko "radice inesistente → rc 1"
# zero file eseguiti non è "verde": un filtro sbagliato non deve mascherare la suite
"$GMB" minitests --root "$GOOD" --filter non_esiste_nessuno > /dev/null 2>&1
[ "$?" = "1" ] && pass "filtro senza risultati → rc 1" || ko "filtro senza risultati → rc 1"

# ── 6/7: JSON valido, failures ordinate, run deterministico ──────────────────────────────
if jq empty "$WORK/bad.json" 2>/dev/null; then
    pass "--json è JSON valido"
else
    ko "--json è JSON valido"
fi
# ordine alfabetico e conteggi coerenti (jq)
if jq -e '([.failures[].file] as $f | ($f == ($f|sort)) and (($f|length) == (.driver_fail + .cc_fail)))' "$WORK/bad.json" >/dev/null 2>&1; then
    pass "failures ordinate e coerenti coi conteggi"
else
    ko "failures ordinate e coerenti coi conteggi"
fi

"$GMB" minitests --root "$BAD" --json > "$WORK/bad2.json" 2>/dev/null
cmp -s "$WORK/bad.json" "$WORK/bad2.json" \
    && pass "due run ⇒ JSON identico" || ko "due run ⇒ JSON identico"

# ── 8: --baseline: rosso solo per le REGRESSIONI, non per il pavimento noto ──────────────
# Il pavimento (84 file) non è 0 e non lo sarà presto: senza baseline il gate è rosso sempre
# e nessuno lo guarda. Con il JSON di un run precedente conta solo ciò che è CAMBIATO.
"$GMB" minitests --root "$BAD" --baseline "$WORK/bad.json" --json > "$WORK/b_same.json" 2>/dev/null
rc=$?
[ "$rc" = "0" ] && pass "baseline identico → rc 0" || { ko "baseline identico → rc 0 (rc=$rc)"; cat "$WORK/b_same.json"; }
[ "$(json_get "$WORK/b_same.json" new_failures)" = "0" ] \
    && pass "baseline identico: 0 nuove regressioni" || ko "baseline identico: 0 nuove regressioni"

"$GMB" minitests --root "$GOOD" --baseline "$WORK/bad.json" --json > "$WORK/b_fixed.json" 2>/dev/null
rc=$?
[ "$rc" = "0" ] && pass "file riparato → rc 0" || ko "file riparato → rc 0 (rc=$rc)"
[ "$(json_get "$WORK/b_fixed.json" fixed)" = "1" ] \
    && pass "file riparato contato in fixed" || ko "file riparato contato in fixed (fixed=$(json_get "$WORK/b_fixed.json" fixed))"

"$GMB" minitests --root "$BAD" --baseline "$WORK/good.json" > /dev/null 2> "$WORK/b_new.err"
rc=$?
[ "$rc" = "2" ] && pass "nuovo FAIL vs baseline → rc 2" || ko "nuovo FAIL vs baseline → rc 2 (rc=$rc)"
grep -q '^REGRESSIONE .*rotto\.bas$' "$WORK/b_new.err" \
    && pass "la regressione è nominata" || ko "la regressione è nominata"

# Il baseline può arrivare da un checkout con la cartella scritta in altro modo
# (`MiniTests/` vs `minitests/`): il confronto deve essere case-insensitive, altrimenti su
# un'altra macchina tutti i file noti sembrano regressioni.
# path reale (`pwd -P`): il CLI stampa i path già risolti (/var → /private/var su macOS)
printf '{"failures":[{"file":"%s"}]}\n' \
    "$(printf '%s' "$(cd "$BAD" && pwd -P)/rotto.bas" | tr 'a-z' 'A-Z')" > "$WORK/base_upper.json"
"$GMB" minitests --root "$BAD" --baseline "$WORK/base_upper.json" --json > "$WORK/b_upper.json" 2>/dev/null
rc=$?
[ "$rc" = "0" ] && pass "baseline con case diverso → rc 0" || ko "baseline con case diverso → rc 0 (rc=$rc)"
[ "$(json_get "$WORK/b_upper.json" new_failures)" = "0" ] \
    && pass "baseline case-insensitive: 0 regressioni" || ko "baseline case-insensitive: 0 regressioni"

echo "---------------------------------"
if [ "$fail" = "0" ]; then echo "gmb minitests: PASS"; else echo "gmb minitests: FAIL"; fi
exit "$fail"
