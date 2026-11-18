#!/usr/bin/env bash
# tools/sh/minitest_parity_perdomain.sh — gambe GESTITE (IL/PCode/VM-C), un PROCESSO per dominio.
#
# PERCHÉ ESISTE: `tools/sh/test_minitests_parity.sh --all` lancia il cross-leg in un unico
# processo xUnit. Lo stato trapela fra un file e l'altro (sottoscrizioni finestre, registry,
# cache FFI) e il processo **crasha** sui domini grafici: i risultati aggregati non sono
# affidabili (documentato in `Bugs/(solved) - MiniTests-Sweep-4Backend-2026-11-07` §10.2).
# Il metodo corretto è UN PROCESSO PER DOMINIO; per i domini che fanno crashare l'host si
# riprova un FILE per processo.
#
# La gamba NATIVA (C99Native, riferimento) la esegue `tools/sh/test_minitests_parity.sh` o
# `tools/sh/test_minitests_c99native`: qui si misurano SOLO le tre gambe gestite.
#
# Uso:
#   bash tools/sh/minitest_parity_perdomain.sh                 # tutti i domini, log in tools/reports/
#   bash tools/sh/minitest_parity_perdomain.sh --only tui,ai    # sottoinsieme
#   bash tools/sh/minitest_parity_perdomain.sh --log <file>
#   bash tools/sh/minitest_parity_perdomain.sh --retry-crash no
#
# Exit: 0 = nessuna divergenza · 1 = almeno una · 2 = setup (binari del test project assenti).
# Nessuna variabile d'ambiente come parametro: solo flag (SDL_VIDEODRIVER=dummy è dell'ambiente
# di test headless, non un parametro di questo tool).
set -uo pipefail

WS="$(cd "$(dirname "$0")/../.." && pwd)"
TESTS="$WS/GMBasicSharp/tests/GMBASIC.IlBackend.Tests"
MINI="$WS/shared/minitests"
LOG="$WS/tools/reports/minitests-parity-perdomain-$(date +%Y-%m-%d).log"
ONLY=""
RETRY=1

while [ $# -gt 0 ]; do
    case "$1" in
        --only)  ONLY="${2:?}"; shift 2 ;;
        --only=*) ONLY="${1#*=}"; shift ;;
        --log)   LOG="${2:?}"; shift 2 ;;
        --log=*) LOG="${1#*=}"; shift ;;
        --retry-crash) RETRY="${2:?}"; shift 2 ;;
        -h|--help) awk 'NR > 1 && /^#/ { sub(/^# ?/, ""); print; next } NR > 1 { exit }' "$0"; exit 0 ;;
        *) echo "opzione sconosciuta: $1 (usa --help)" >&2; exit 2 ;;
    esac
done

[ -d "$TESTS/bin" ] || { echo "ERRORE: test project non buildato ($TESTS/bin). Rilancia: dotnet build $TESTS --no-restore" >&2; exit 2; }

mkdir -p "$(dirname "$LOG")"
: > "$LOG"
say() { echo "$*" | tee -a "$LOG"; }

FILTER="FullyQualifiedName~MiniTests_IL_PCode_Parity"
domains=()
for d in "$MINI"/*/; do
    [ -d "$d" ] || continue
    dom="$(basename "$d")"
    if [ -n "$ONLY" ]; then
        case ",$ONLY," in *",$dom,"*) ;; *) continue ;; esac
    fi
    # Un dominio SENZA `.bas` non è un dominio: il filtro `dom/` non corrisponderebbe a nulla e
    # l'harness lo segnala come errore («filtro errato»). Es.: `ffi_pending/`, `optimize/`.
    set -- "$d"*.bas
    [ -e "$1" ] || { echo "# salto $dom (nessun .bas)" | tee -a "$LOG"; continue; }
    domains+=("$dom")
done
say "# gambe gestite (IL+PCode+VM-C), un processo per dominio — $(date '+%Y-%m-%d %H:%M')"
say "# domini: ${#domains[@]}"

# BROKER pub/sub: i MiniTest `messaging/` (tcp) senza broker divergono per costruzione. Si
# riusa quello già attivo (lease), se no lo si avvia come fa `test_minitests_parity.sh`.
BROKER_SH="$WS/tools/sh/msg_broker.sh"
if [ -x "$BROKER_SH" ]; then
    src="/"
    for p in $("$BROKER_SH" ports 2>/dev/null); do
        [ -n "$p" ] || continue
        r="$("$BROKER_SH" ensure "$p" "" "" "$$" 2>/dev/null)" || r=failed
        case "$r" in
            started) say "# broker :$p avviato (lease $$)" ;;
            already) say "# broker :$p già attivo (riuso)" ;;
            *)       say "# attenzione: broker :$p non disponibile ($r)" ;;
        esac
    done
fi

run_one() {  # <filtro>  → stdout del dotnet test
    SDL_VIDEODRIVER=dummy GMB_MINI_ONLY="$1" dotnet test "$TESTS" --no-build --filter "$FILTER" 2>&1
}

pass=0; fail=0; crash=0; skipped=0
failed_domains=()
for dom in "${domains[@]}"; do
    out="$(run_one "$dom/")"
    if echo "$out" | grep -qE "Arresto anomalo del processo host|Test Run Aborted|crash"; then
        if [ "$RETRY" = 1 ]; then
            say "CRASH-HOST $dom → riprovo un FILE per processo"
            for f in "$MINI/$dom"/*.bas; do
                [ -f "$f" ] || continue
                b="$(basename "$f")"
                o2="$(run_one "$dom/$b")"
                if echo "$o2" | grep -q "Non superato"; then
                    say "FAIL $dom/$b"; fail=$((fail + 1)); failed_domains+=("$dom/$b")
                elif echo "$o2" | grep -qE "Arresto anomalo|Test Run Aborted"; then
                    say "CRASH $dom/$b"; crash=$((crash + 1)); failed_domains+=("$dom/$b (crash)")
                elif echo "$o2" | grep -q "Superati:"; then
                    pass=$((pass + 1))
                else
                    skipped=$((skipped + 1))
                fi
            done
        else
            say "CRASH $dom (host)"; crash=$((crash + 1)); failed_domains+=("$dom (crash host)")
        fi
        continue
    fi
    nonpass="$(echo "$out" | grep -oE "Non superati: *[0-9]+" | grep -oE "[0-9]+" | head -1)"
    passati="$(echo "$out" | grep -oE "Superati: *[0-9]+" | grep -oE "[0-9]+" | head -1)"
    [ -n "$nonpass" ] || nonpass=0
    [ -n "$passati" ] || passati=0
    if [ "$nonpass" -gt 0 ]; then
        say "FAIL $dom ($nonpass)"; fail=$((fail + nonpass)); failed_domains+=("$dom")
        { echo "===== $dom ====="; echo "$out"; } >> "$LOG"
    else
        pass=$((pass + passati))
    fi
done

say "# ---- riepilogo ----"
say "# PASS=$pass FAIL=$fail CRASH=$crash SKIP=$skipped (domini ${#domains[@]})"
if [ "${#failed_domains[@]}" -gt 0 ]; then
    say "# domini con almeno un fallimento:"
    for d in "${failed_domains[@]}"; do say "#   $d"; done
    say "# log completo: $LOG"
    exit 1
fi
say "# nessuna divergenza: OK"
exit 0
