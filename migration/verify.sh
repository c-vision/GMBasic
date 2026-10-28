#!/usr/bin/env bash
# migration/verify.sh — UNICO punto d'ingresso per i controlli di migration/.
#
# Perché esiste: i controlli di migration/ erano sparsi (script di test dentro
# tools-unclassified/audit, gate storici importati dal repo originale) e ognuno andava
# invocato a mano, sapendo quale funzionava e quale no. Qui sono raccolti in gruppi con
# esito esplicito, e i controlli che NON sono eseguibili da questa posizione sono elencati
# col motivo invece di essere silenziosamente saltati o finti verdi.
#
# Uso:
#   bash migration/verify.sh              # tutti i gruppi (statici + audit)
#   bash migration/verify.sh --list       # elenca i gruppi e ciò che verificano
#   bash migration/verify.sh --no-syntax  # salta i controlli statici (solo audit)
#   bash migration/verify.sh --tools DIR  # esegue i gruppi su un'altra copia di
#                                         # tools-unclassified (serve a validare il gate)
#   bash migration/verify.sh --c99native DIR  # compila un'altra copia del C99Native
#                                         # (serve a validare il gruppo build)
#   AUDIT_SRC=/path/Program.cs bash migration/verify.sh   # gruppo audit su un'altra copia
#                                         # del sorgente dell'audit (validazione del gate)
#
# Exit: 0 = nessun FAIL · 1 = almeno un FAIL · 2 = nessun FAIL ma un gruppo non
#       eseguibile per setup mancante (es. dotnet assente) o uso errato.
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TOOLS="$SCRIPT_DIR/tools-unclassified"

RUN_SYNTAX=1
C99NATIVE="$ROOT/GMBasic/native/gmb_c99native"
while [ $# -gt 0 ]; do
    case "$1" in
        --list)
            cat <<'EOF'
Gruppi di verifica di migration/

  build     Compila il backend C99Native (src/gmb_c99_emit*.c + driver) con cc -c in una
            dir temporanea, e compila l'audit (--build-only). Intercetta le rotture di
            build prima dei test. Non configura CMake e non scrive oggetti nel repo.
  statici   Sintassi di tutti gli script della dump: bash -n su ogni .sh e compile()
            su ogni .py. Non esegue nulla, non tocca il repo, nessun artefatto.
  audit     tools-unclassified/audit/test_retired_flags.sh — i flag ritirati dell'audit
            (--c99, --registry-c99) devono uscire con 2 e non produrre file, e
            --registry-c99native deve restare vivo. Compila il vero Program.cs in una
            dir temporanea (l'audit.csproj della dump non è compilabile in place).

Controlli NOTI come non eseguibili da migration/ (elencati, non eseguiti):
  check_file_size.sh   assume ROOT = dirname($0)/.. = il repo originale (tools/ alla
                       radice). Da migration/tools-unclassified/ la radice sarebbe
                       migration/, quindi scandirebbe il nulla. Gira nel repo originale.
  check_minitests.sh   stesso assunto di ROOT e in più richiede la CLI nativa
                       buildata (native/gmb_compiler_c/build/gmb) + un run completo dei
                       MiniTest: non è un controllo "sparsi" leggero, e i run massivi
                       richiedono l'ok esplicito dell'utente.
EOF
            exit 0 ;;
        --no-syntax) RUN_SYNTAX=0; shift ;;
        --tools)     TOOLS="${2:-}"; shift 2 ;;
        --c99native) C99NATIVE="${2:-}"; shift 2 ;;
        -h|--help)   sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "opzione sconosciuta: $1 (usa --list)" >&2; exit 2 ;;
    esac
done

[ -d "$TOOLS" ] || { echo "FATAL: tools-unclassified non trovato: $TOOLS" >&2; exit 2; }

WORK="$(mktemp -d "${TMPDIR:-/tmp}/gmb_migration_verify.XXXXXX")"
cleanup() { rm -rf "$WORK"; }
trap cleanup EXIT

RESULTS="$WORK/results"; : > "$RESULTS"
FAILS=0
SETUPS=0

record() {  # record <PASS|FAIL|SKIP|INFO> <nome> <dettaglio>
    printf '%s\t%s\t%s\n' "$1" "$2" "$3" >> "$RESULTS"
    case "$1" in
        FAIL) FAILS=$((FAILS + 1)) ;;
        SKIP) SETUPS=$((SETUPS + 1)) ;;
    esac
}

# ── gruppo: build ─────────────────────────────────────────────────────────────
# Compila il C99Native (emitter + driver) a oggetto in una dir temporanea: non usa CMake,
# non tocca le build dir condivise (contesa con altre AI) e non lascia artefatti nel repo.
group_build_c99native() {
    if ! command -v cc >/dev/null 2>&1; then
        record SKIP "build:C99Native" "cc non nel PATH"
        return
    fi
    if [ ! -d "$C99NATIVE/src" ]; then
        record SKIP "build:C99Native" "sorgenti non trovati: $C99NATIVE/src"
        return
    fi

    local inc=(-I"$C99NATIVE/include"
               -I"$C99NATIVE/../gmb_compiler_c/frontend/include"
               -I"$C99NATIVE/../gmb_runtime_c/include"
               -I"$C99NATIVE/../gmb_bytecode/include")
    local objdir="$WORK/obj"
    mkdir -p "$objdir"

    local n=0 bad=0 f detail=""
    while IFS= read -r f; do
        n=$((n + 1))
        if ! cc -c "${inc[@]}" "$f" -o "$objdir/$(basename "$f" .c).o" 2>"$WORK/cc.err"; then
            bad=$((bad + 1))
            detail="$detail  $(basename "$f"): $(grep -m1 -E 'error' "$WORK/cc.err" || head -1 "$WORK/cc.err")"$'\n'
        fi
    done < <(find "$C99NATIVE/src" "$C99NATIVE/driver" -type f -name '*.c' | sort)

    if [ "$bad" -eq 0 ]; then
        record PASS "build:C99Native" "$n file compilati (emitter + driver)"
    else
        detail="${detail%$'\n'}"
        record FAIL "build:C99Native" "$bad/$n file in errore"$'\n'"$detail"
    fi
}

group_build_audit() {
    local script="$TOOLS/audit/test_retired_flags.sh"
    if [ ! -f "$script" ]; then
        record SKIP "build:audit" "test_retired_flags.sh assente"
        return
    fi
    if ! command -v dotnet >/dev/null 2>&1; then
        record SKIP "build:audit" "dotnet non nel PATH"
        return
    fi
    local out rc
    out="$(bash "$script" --build-only 2>&1)"
    rc=$?
    case "$rc" in
        0) record PASS "build:audit" "$(printf '%s' "$out" | grep -E '^build ok' | head -1)" ;;
        1) record FAIL "build:audit" "$(printf '%s' "$out" | grep -m1 -E 'FAIL|error' | sed 's/^FAIL  //')" ;;
        *) record SKIP "build:audit" "non eseguibile (exit $rc)" ;;
    esac
}

# ── gruppo: statici ───────────────────────────────────────────────────────────
group_syntax() {
    local total=0 bad=0 f rel msg detail=""
    while IFS= read -r f; do
        total=$((total + 1))
        if ! bash -n "$f" 2>"$WORK/syntax.err"; then
            bad=$((bad + 1))
            rel="$(printf '%s' "$f" | sed "s|^$TOOLS/||")"
            msg="$(head -1 "$WORK/syntax.err" | sed "s|$TOOLS/||g")"
            detail="$detail  $rel: ${msg#"$rel": }"$'\n'
        fi
    done < <(find "$TOOLS" -type f -name '*.sh' | sort)

    local py=0 pybad=0
    while IFS= read -r f; do
        py=$((py + 1))
        if ! python3 -c 'import sys; compile(open(sys.argv[1], encoding="utf-8").read(), sys.argv[1], "exec")' "$f" 2>"$WORK/py.err"; then
            pybad=$((pybad + 1))
            rel="$(printf '%s' "$f" | sed "s|^$TOOLS/||")"
            msg="$(head -1 "$WORK/py.err" | sed "s|$TOOLS/||g")"
            detail="$detail  $rel: ${msg#"$rel": }"$'\n'
        fi
    done < <(find "$TOOLS" -type f -name '*.py' | sort)

    local sb="$total script shell, $py Python"
    if [ "$bad" -eq 0 ] && [ "$pybad" -eq 0 ]; then
        record PASS "statici" "$sb — sintassi valida"
    else
        detail="${detail%$'\n'}"          # niente riga vuota in coda
        record FAIL "statici" "$sb — $((bad + pybad)) con errori di sintassi"$'\n'"$detail"
    fi
}

# ── gruppo: audit ─────────────────────────────────────────────────────────────
group_audit() {
    local script="$TOOLS/audit/test_retired_flags.sh"
    if [ ! -f "$script" ]; then
        record SKIP "audit" "test_retired_flags.sh assente"
        return
    fi
    if ! command -v dotnet >/dev/null 2>&1; then
        record SKIP "audit" "dotnet non nel PATH"
        return
    fi

    local out rc
    # AUDIT_SRC: passthrough opzionale al test (serve a validare il gate su un'altra
    # copia del sorgente: su una copia pre-rimozione il gruppo DEVE risultare FAIL).
    if [ -n "${AUDIT_SRC:-}" ]; then
        out="$(AUDIT_SRC="$AUDIT_SRC" bash "$script" 2>&1)"
    else
        out="$(bash "$script" 2>&1)"
    fi
    rc=$?
    case "$rc" in
        0)
            record PASS "audit" "$(printf '%s' "$out" | grep -E '^[0-9]+ passati' || echo 'ok')" ;;
        1)
            # riporta solo le righe FAIL del test, non tutto l'output
            record FAIL "audit" "$(printf '%s' "$out" | grep -E '^(FAIL|[0-9]+ passati)' | tr '\n' ' ')" ;;
        *)
            record SKIP "audit" "non eseguibile (exit $rc): $(printf '%s' "$out" | grep -m1 -E 'FATAL|error|syntax' || printf '%s' "$out" | head -1)" ;;
    esac
}

# ── esecuzione ────────────────────────────────────────────────────────────────
# build prima dei test: una rottura di compilazione si vede subito e non sporca i test
group_build_c99native
group_build_audit
[ "$RUN_SYNTAX" -eq 1 ] && group_syntax
group_audit

# ── riepilogo ─────────────────────────────────────────────────────────────────
printf '\n================= migration/verify =================\n'
while IFS=$'\t' read -r status name detail; do
    printf '%-5s %-8s %s\n' "$status" "$name" "$detail"
done < "$RESULTS"
echo '---------------------------------------------------'
echo 'NON eseguibili da migration/ (per scelta, non per errore):'
echo '      check_file_size.sh    ROOT = dirname($0)/.. → nel repo originale, non qui'
echo '      check_minitests.sh    richiede CLI nativa + run completo MiniTest'
echo '==================================================='

if [ "$FAILS" -gt 0 ]; then
    echo "ESITO: FALLITO ($FAILS gruppi con FAIL)"
    exit 1
fi
if [ "$SETUPS" -gt 0 ]; then
    echo "ESITO: INCOMPLETO ($SETUPS gruppi non eseguibili)"
    exit 2
fi
echo "ESITO: OK"
exit 0
