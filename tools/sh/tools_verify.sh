#!/usr/bin/env bash
# tools/sh/tools_verify.sh — verifiche statiche/build del contenuto di tools/.
#
# Provenienza: era `migration/verify.sh` (unico punto d'ingresso dei controlli di
# migration/, quando i tool erano sparsi in tools-unclassified/). Dopo la consolidazione
# del 2026-10-29 esiste UNA sola sede (`tools/` alla radice) e migration/tools-unclassified
# non esiste piu': i gruppi che la usavano (build:audit, audit) sono stati rimossi —
# restano i due che hanno senso sul layout attuale.
#
# Uso:
#   bash tools/sh/tools_verify.sh            # tutti i gruppi (build + statici)
#   bash tools/sh/tools_verify.sh --list     # elenca i gruppi e ciò che verificano
#   bash tools/sh/tools_verify.sh --no-syntax  # salta i controlli statici (solo build)
#   bash tools/sh/tools_verify.sh --c99native DIR  # compila un'altra copia del C99Native
#                                         # (serve a validare il gruppo build)
#
# Exit: 0 = nessun FAIL · 1 = almeno un FAIL · 2 = nessun FAIL ma un gruppo non
#       eseguibile per setup mancante (es. cc assente) o uso errato.
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TOOLS="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT="$(cd "$TOOLS/.." && pwd)"

RUN_SYNTAX=1
C99NATIVE="$ROOT/GMBasic/native/gmb_c99native"
while [ $# -gt 0 ]; do
    case "$1" in
        --list)
            cat <<'EOF'
Gruppi di verifica di tools/

  build     Compila il backend C99Native (src/gmb_c99_emit*.c + driver) con cc -c in una
            dir temporanea. Intercetta le rotture di build prima dei test. Non configura
            CMake e non scrive oggetti nel repo.
  statici   Sintassi di tutti gli script di tools/: bash -n su ogni .sh e compile()
            su ogni .py. Non esegue nulla, non tocca il repo, nessun artefatto.

Controlli NON coperti qui (esistono come script propri, invocabili singolarmente):
  tools/sh/check_file_size.sh          dimensioni file + escape illegali BASIC
  tools/sh/check_minitests.sh          richiede la CLI nativa buildata e N run MiniTest
  tools/sh/test_minitests_parity       parity 4 gambe (chirurgica: un file per volta)
EOF
            exit 0 ;;
        --no-syntax) RUN_SYNTAX=0; shift ;;
        --c99native) C99NATIVE="${2:-}"; shift 2 ;;
        -h|--help)   sed -n '2,23p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "opzione sconosciuta: $1 (usa --list)" >&2; exit 2 ;;
    esac
done

[ -d "$TOOLS" ] || { echo "FATAL: tools/ non trovato: $TOOLS" >&2; exit 2; }

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
        pybad=$((pybad + 1))
        rel="$(printf '%s' "$f" | sed "s|^$TOOLS/||")"
        detail="$detail  $rel: tool Python vietato (policy: niente Python in tools/)"$'\n'
    done < <(find "$TOOLS" -type f -name '*.py' | sort)

    local sb="$total script shell"
    [ "$py" -gt 0 ] && sb="$sb, $py Python (VIETATI)"
    if [ "$bad" -eq 0 ] && [ "$pybad" -eq 0 ]; then
        record PASS "statici" "$sb — sintassi valida"
    else
        detail="${detail%$'\n'}"          # niente riga vuota in coda
        record FAIL "statici" "$sb — $((bad + pybad)) con errori di sintassi"$'\n'"$detail"
    fi
}

# ── esecuzione ────────────────────────────────────────────────────────────────
# build prima dei test: una rottura di compilazione si vede subito e non sporca i test
group_build_c99native
[ "$RUN_SYNTAX" -eq 1 ] && group_syntax

# ── riepilogo ─────────────────────────────────────────────────────────────────
printf '\n================= tools/verify =====================\n'
while IFS=$'\t' read -r status name detail; do
    printf '%-5s %-8s %s\n' "$status" "$name" "$detail"
done < "$RESULTS"
echo '---------------------------------------------------'
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
