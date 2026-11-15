#!/usr/bin/env bash
# check_minitests.sh — GATE dei MiniTests contro il baseline REGISTRATO nel repo.
#
# PERCHÉ ESISTE: il pavimento di fallimenti C99Native non è 0 (83 file al 2026-10-24: 36
# CC-FAIL + 47 DRIVER-FAIL), quindi un gate "verde se 0 FAIL" sarebbe rosso sempre e nessuno
# lo guarderebbe. Questo script confronta il run corrente col baseline versionato e fallisce
# solo per le REGRESSIONI (un FAIL che prima non c'era). Il pavimento che scende non è un
# errore: appare come `RISOLTO <file>` e col tempo si rigenera il baseline.
#
# I MiniTest NEGATIVI di compilazione (`' EXPECT: ERROR: <testo>`) NON sono fallimenti: il CLI
# conta PASS il rifiuto atteso del driver, quindi non entrano nel baseline e aggiungerne uno
# nuovo non richiede `--update`.
#
# Uso:
#   tools/sh/check_minitests.sh               # verifica → exit 0 ok · 2 regressione · 1 setup
#   tools/sh/check_minitests.sh --update      # rigenera il baseline (dopo fix o nuovi MiniTest)
#   tools/sh/check_minitests.sh --json        # pass-through: JSON su stdout, riepilogo su stderr
#   tools/sh/check_minitests.sh --filter tui/ # sottoinsieme (NON per il gate: vedi avviso)
#
# Env: GMB_CLI (CLI alternativo), GMB_MINITESTS_BASELINE (baseline alternativo).
#
# In CI basta invocare questo script dopo la build di `gmb`:
#   cmake -S native/gmb_compiler_c -B native/gmb_compiler_c/build
#   cmake --build native/gmb_compiler_c/build --target gmb_cli
#   tools/sh/check_minitests.sh
# Nota: il `cmake -S` include il modello MoE da LLMEnv (checkout sibling), quindi la pipeline
# deve avere anche quello; senza LLMEnv il configure fallisce prima di arrivare al gate.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/../../GMBasic" && pwd)"
# I TOOL vivono nella radice del MONOREPO (`tools/`), non più in GMBasic/tools.
MONO="$(cd "$(dirname "$0")/../.." && pwd)"
GMB="${GMB_CLI:-$ROOT/native/gmb_compiler_c/build/gmb}"
BASELINE="${GMB_MINITESTS_BASELINE:-$ROOT/native/gmb_compiler_c/tests/minitests_baseline.json}"

# ── GATE 0: parità delle due implementazioni SemVer (compilatore vs resolver/PM) ──────────
# PERCHÉ QUI: la versione di un pacchetto viene scelta in DUE mondi che non possono linkarsi
# (frontend del compilatore e resolver del Package Manager). Una divergenza produce "lo stesso
# REQUIRE sceglie versioni diverse a seconda dello strumento" — è già successo col build
# metadata ("1.2.3+b5" non soddisfaceva "1.2.3"). Il gate confronta i comportamenti su un corpus
# e NON richiede la build del CLI, quindi va per primo.
if [ -x "$MONO/tools/sh/pkg_semver_parity.sh" ]; then
    "$MONO/tools/sh/pkg_semver_parity.sh" -q || {
        echo "GATE FALLITO: pkg_semver_parity — il compilatore e il resolver scelgono versioni diverse"
        exit 1
    }
fi

[ -x "$GMB" ] || {
    echo "gmb mancante: $GMB"
    echo "build: cmake --build native/gmb_compiler_c/build --target gmb_cli"
    exit 1
}

update=0
args=()
for a in "$@"; do
    case "$a" in
        --update) update=1 ;;
        *) args+=("$a") ;;
    esac
done

# RADICE dei MiniTest: nel repo originale stanno in <install>/MiniTests, nel monorepo in
# <repo>/shared/minitests (fratello di GMBasic/). Senza questo, `gmb minitests` falliva con
# "radice non trovata" e il gate non era eseguibile nel monorepo.
ROOT_MINI=""
for cand in "$ROOT/../shared/minitests" "$ROOT/MiniTests"; do
    if [ -d "$cand" ]; then ROOT_MINI="$cand"; break; fi
done
[ -n "$ROOT_MINI" ] && args+=(--root "$ROOT_MINI")

count_noti() { # numero di FAIL nel baseline (0 se il file non esiste)
    [ -f "$BASELINE" ] || { echo 0; return; }
    grep -c '"file"' "$BASELINE" 2>/dev/null || true
}

if [ "$update" = "1" ]; then
    dir="$(dirname "$BASELINE")"
    mkdir -p "$dir"
    tmp="$BASELINE.tmp.$$"
    echo "gmb minitests: rigenero il baseline in $BASELINE" >&2
    # rc=2 (ci sono FAIL) è il caso NORMALE di un baseline: è il pavimento. Solo rc=1 (setup
    # rotto: driver/radice mancanti) o un JSON vuoto devono lasciare il baseline com'era.
    "$GMB" minitests --json ${args[@]+"${args[@]}"} > "$tmp"
    rc=$?
    if [ "$rc" = "1" ] || [ ! -s "$tmp" ]; then
        rm -f "$tmp"
        echo "gmb minitests: rigenerazione fallita (rc=$rc): baseline invariato" >&2
        exit 1
    fi
    # scrittura atomica: un'interruzione non deve lasciare un baseline troncato
    mv "$tmp" "$BASELINE"
    echo "gmb minitests: baseline aggiornato — $(count_noti) FAIL noti" >&2
    exit 0
fi

[ -f "$BASELINE" ] || {
    echo "gmb minitests: baseline mancante: $BASELINE" >&2
    echo "crealo con: tools/sh/check_minitests.sh --update" >&2
    exit 1
}

echo "gmb minitests: baseline $BASELINE — $(count_noti) FAIL noti" >&2
# `${args[@]+...}` e non "${args[@]}": con `set -u` bash 3.2 (macOS) dichiara `args[@]`
# non impostato quando non ci sono argomenti — cioè proprio il caso della CI.
"$GMB" minitests --baseline "$BASELINE" ${args[@]+"${args[@]}"}
rc=$?
case "$rc" in
    0) echo "gmb minitests: gate PASS (nessuna regressione sul baseline)" >&2 ;;
    2) echo "gmb minitests: gate FAIL (regressioni: vedi REGRESSIONE sopra)" >&2 ;;
    *) echo "gmb minitests: gate non eseguito (rc=$rc: setup rotto)" >&2 ;;
esac
exit "$rc"
