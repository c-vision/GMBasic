#!/bin/zsh
# test_minitests_parity — esecuzione MIRATA dei MiniTest su 4 gambe (IL, PCode, VM-C, C99Native).
# ORDINE E RUOLI: prima la gamba NATIVA (C99Native, runner nativo) = RIFERIMENTO — il
# compilatore principale del monorepo è quello nativo; poi le tre gambe gestite (IL, PCode,
# VM-C, harness xUnit) = verifica di PARITÀ. Il backend C# C99 non esiste più.
# Uso: tools/sh/test_minitests_parity.sh <sottostringa> [<altra> ...] | --all | --interactive [filtri]
# I filtri sono una LISTA (match ANY) su TUTTE e 4 le gambe: gli argomenti vengono uniti con
# virgola in GMB_MINI_ONLY e sia l'xUnit sia il runner nativo la leggono come lista. Un filtro
# che non corrisponde ad alcun file fa FALLIRE la gamba nativa (exit 1): prima la gamba
# stampava `PASS=0 FAIL=0` con exit 0, cioè un falso verde.
set -e
ROOT="$(cd "$(dirname "$0")/../../GMBasicSharp" && pwd)"          # GMBasicSharp
WS="$(cd "$ROOT/.." && pwd)"                       # GMBasicMonoRepo

unset GMB_MINI_ONLY GMB_RUN_ALL
case "$1" in
  --all) export GMB_RUN_ALL=1 ;;
  --interactive) export GMB_RUN_ALL=1; export GMB_RUN_INTERACTIVE=1 ;;
  "") echo "uso: test_minitests_parity.sh <sottostringa> [...] | --all | --interactive" >&2; exit 2 ;;
  *) export GMB_MINI_ONLY="${(j:,:)@}" ;;
esac

flags="--no-build"
[ "${GMB_MINITEST_BUILD:-0}" = "1" ] && flags=""

# RID della macchina (per i path delle librerie native).
case "$(uname -s)-$(uname -m)" in
  Darwin-arm64) RID=osx-arm64 ;;
  Darwin-x86_64) RID=osx-x64 ;;
  Linux-x86_64) RID=linux-x64 ;;
  Linux-aarch64|Linux-arm64) RID=linux-arm64 ;;
  *) RID=osx-arm64 ;;
esac
# ORDINE = il codice che si sta testando PRIMA della cache: `build/libs/<rid>` sono le librerie
# appena costruite (AI/engine comprese), `~/.gmbasic/libs` è la cache dei bundle PUBBLICATI.
# Con la cache davanti, una lib pubblicata più vecchia dei sorgenti veniva caricata in SILENZIO
# dalle gambe IL/PCode mentre VM/C e C99 (link statici) usavano il build locale → divergenze
# fantasma su llm/* (caso reale 2026-10-29: `LLM TENSOR INFO` con `arch=` vuoto su IL/PCode e
# `arch=qwen3_moe` su VM/C; con build/libs davanti: PASS su tutte le gambe).
if [ "$(uname)" = "Darwin" ]; then
  export DYLD_LIBRARY_PATH="$WS/GMBasic/build/libs/$RID:$HOME/.gmbasic/libs/ml/$RID:$HOME/.gmbasic/libs/$RID:${DYLD_LIBRARY_PATH:-}"
fi
export GMBASIC_ROOT="$WS/GMBasic"

# ── broker pub/sub: lo avvia e lo ferma il GATE, non l'utente ────────────────────────────────
# `ports` è autodescrittivo: legge i MiniTest SELEZIONATI e prende le porte dai loro `tcp://`
# loopback (oggi messaging/message_tcp_parity.bas e message_close_parity.bas su :9215). Prima il
# broker era un prerequisito manuale: se era morto i test tcp fallivano con "timeout" e restava
# ambiguo se la consegna verificata fosse quella cross-connessione o un altro percorso. `ensure`
# non tocca un broker già attivo (lo lascia a chi l'ha avviato), `release` ferma SOLO i nostri.
BROKER_SH="$WS/tools/sh/msg_broker.sh"
BROKER_STARTED=()
broker_cleanup() {
  local p
  for p in "${BROKER_STARTED[@]}"; do
    [ -n "$p" ] && "$BROKER_SH" release "$p" >/dev/null 2>&1
  done
}
trap broker_cleanup EXIT          # ferma il broker anche se un test fallisce
# INT/TERM: non intercettarli (swallow) — si esce, e l'EXIT fa la pulizia.
trap 'exit 130' INT
trap 'exit 143' TERM
if [ -x "$BROKER_SH" ]; then
  if [ -n "${GMB_MINI_ONLY:-}" ]; then
    broker_ports=("${(@f)$("$BROKER_SH" ports "${(s:,:)GMB_MINI_ONLY}")}")
  else
    broker_ports=("${(@f)$("$BROKER_SH" ports)}")
  fi
  for _bp in "${broker_ports[@]}"; do
    [ -n "$_bp" ] || continue
    _bres="$("$BROKER_SH" ensure "$_bp" 2>/dev/null)" || _bres=failed
    case "$_bres" in
      started) BROKER_STARTED+=("$_bp"); echo "broker pub/sub avviato dal gate su :$_bp" ;;
      already) echo "broker pub/sub già attivo su :$_bp (lo lascio a chi l'ha avviato)" ;;
      *)       echo "attenzione: broker non avviabile su :$_bp" >&2 ;;
    esac
  done
else
  echo "attenzione: $BROKER_SH assente — i MiniTest tcp non hanno il loro broker" >&2
fi

# --- gamba NATIVA (C99Native): è il RIFERIMENTO — il compilatore principale del monorepo è
# --- quello nativo. Le gambe IL/PCode/VM-C girano dopo, come verifica di PARITÀ.
if [ -n "${GMB_MINI_ONLY:-}" ]; then
  echo "gamba NATIVA (riferimento): C99Native — poi parità su IL + PCode + VM-C (xUnit)"
  "$WS/tools/sh/test_minitests_c99native"
else
  echo "gamba NATIVA (riferimento): C99Native (suite intera) — poi parità su IL + PCode + VM-C"
  ( unset GMB_MINI_ONLY; "$WS/tools/sh/test_minitests_c99native" )
fi

# --- parità: le tre gambe gestite (IL, PCode, VM-C) sullo stesso filtro ---
# ANTI-STALE: BUILD esplicito prima del run. Le gambe IL/PCode/VM-C usano le DLL del
# compilatore copiate nel bin del test project: se il test project non viene ricostruito
# dopo una modifica al frontend/backend, il gate misura codice VECCHIO (falso verde/rosso).
# `dotnet test` da solo non sempre ricostruisce le dipendenze qui; con build esplicito
# + `--no-build` il run è deterministico. La guardia runtime è in
# `tests/GMBASIC.IlBackend.Tests/BuildFreshness.cs` (fallisce se una DLL resta stale).
# Su una macchina carica il rebuild del test project può dominare il tempo del gate (minuti
# di MSBuild). GMB_SKIP_DOTNET_BUILD=1 salta il build quando le DLL sono già aggiornate
# (es. run ripetuti ravvicinati); il controllo anti-stale resta attivo nella build normale.
if [ "${GMB_SKIP_DOTNET_BUILD:-0}" = "1" ]; then
  echo "build del test project: SALTATO (GMB_SKIP_DOTNET_BUILD=1)"
else
  echo "build del test project (anti-stale)..."
  dotnet build "$ROOT/tests/GMBASIC.IlBackend.Tests" \
    -v q --nologo || exit 1
fi
# ISOLAMENTO PER FILE (default, GMB_MINI_SPLIT=0 per disattivarlo): le gambe IL e PCode
# girano nello STESSO processo del test host. Con molti MiniTest FFI pesanti nello stesso
# processo il runtime può terminare (crash del GC dopo ~6 file: ognuno pinna decine di array
# e crea tipi-delegate): i risultati sono identici, è l'host che muore. Un `dotnet test` per
# file isola il processo e rende il gate deterministico (~2 s di startup per file).
if [ "${GMB_MINI_SPLIT:-1}" = "1" ] && [ -n "${GMB_MINI_ONLY:-}" ]; then
  SPLIT_FAIL=0; SPLIT_N=0
  LIST="$(mktemp "${TMPDIR:-/tmp}/gmb_split.XXXXXX")"
  NEEDLES="$(mktemp "${TMPDIR:-/tmp}/gmb_needles.XXXXXX")"
  printf '%s\n' "$GMB_MINI_ONLY" | tr ',' '\n' | sed '/^$/d' > "$NEEDLES"
  while IFS= read -r f; do
    [ -n "$f" ] || continue
    rel="${f#"$WS/shared/minitests/"}"
    hit=0
    while IFS= read -r n; do
      [ -n "$n" ] || continue
      case "$rel" in *"$n"*) hit=1 ;; esac
    done < "$NEEDLES"
    [ "$hit" = 1 ] && printf '%s\n' "$rel" >> "$LIST"
  done <<EOF
$(find "$WS/shared/minitests" -name '*.bas' -type f | sort)
EOF
  rm -f "$NEEDLES"
  while IFS= read -r rel; do
    [ -n "$rel" ] || continue
    SPLIT_N=$((SPLIT_N+1))
    if ! GMB_MINI_ONLY="$rel" dotnet test "$ROOT/tests/GMBASIC.IlBackend.Tests" \
        --no-build --filter "FullyQualifiedName~MiniTests_IL_PCode_Parity" $flags; then
      SPLIT_FAIL=1
    fi
  done < "$LIST"
  rm -f "$LIST"
  echo "parità gestita: $SPLIT_N file (un processo per file)"
  [ "$SPLIT_FAIL" = 0 ] || exit 1
else
  dotnet test "$ROOT/tests/GMBASIC.IlBackend.Tests" \
    --no-build \
    --filter "FullyQualifiedName~MiniTests_IL_PCode_Parity" \
    $flags
fi
