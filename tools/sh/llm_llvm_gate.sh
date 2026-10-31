#!/usr/bin/env bash
# ── MUTEX di build: i comandi di build (cc/clang/cmake/make/dotnet) prendono un lock ──
# esclusivo su mutex.txt SOLO per la durata del SINGOLO comando, poi lo rilasciano subito
# (lo script puo proseguire a lungo senza tenere il lock). Scadenza 15 min con heartbeat:
# un processo in hang non blocca le altre AI. Vedi tools/gmb_with_lock.sh e Knowledge/
# Build-Mutex-Lock-AI.md.
if [ -z "${GMB_BUILDBIN_ON:-}" ]; then
  export GMB_BUILDBIN_ON=1
  GMB_BUILDBIN="$(cd "$(dirname "$0")" && pwd)/buildbin"
  [ -d "$GMB_BUILDBIN" ] && export PATH="$GMB_BUILDBIN:$PATH"
fi

# tools/sh/llm_llvm_gate.sh — C1: gate di regressione CROSS-DOMINIO della superficie LLM/LLVM.
#
# Scopo: verificare che una modifica a LLM/TRAIN/TORCH/AI NON abbia rotto la superficie
# LLVM (model compiler / surgery / IR / training). UN comando, esito sintetico ACCEPT/REJECT.
#
# ⚠️ GATE PESANTE — NON auto-lanciato (AGENTS §0/§0bis): si invoca ESPLICITAMENTE, in finestra
#    quieta, quando serve (p.es. prima di dichiarare chiusa una modifica LLM/LLVM). Non fa parte
#    di nessun hook/CI automatico.
#
# Uso:
#   tools/sh/llm_llvm_gate.sh                 # CORE: miniTest llm/ + train/ + §4.10 SEMAPRAX + ai LLVM su TUTTI i 4 backend
#   tools/sh/llm_llvm_gate.sh --samples       # + sample AiLLM rappresentativi (via parity4, 4 gambe)
#   tools/sh/llm_llvm_gate.sh --full          # + tutto ai/ + TUTTI i sample AiLLM (heavy/interactive esclusi)
#   tools/sh/llm_llvm_gate.sh --list          # elenca cosa girerebbe, senza eseguire
#   tools/sh/llm_llvm_gate.sh -n              # dry-run: stampa i comandi senza eseguirli
#   tools/sh/llm_llvm_gate.sh --build         # forza la build del progetto test (prima volta)
#   tools/sh/llm_llvm_gate.sh --only <filtro> # test CHIRURGICO su un solo filtro/file (no sample)
#
# Gambe coperte:
#   IL / PCode / VMC   -> tools/test_minitests_parity   (xUnit, in-process)
#   C99Native          -> tools/sh/test_minitests_c99native (lexer/parser/emitter C -> C)
#   Sample             -> tools/sh/parity4 (4 gambe per sample)
#
# Report: per-blocco PASS/FAIL + finale "ACCEPT"/"REJECT". Exit 0=ACCEPT, 1=REJECT, 2=uso.
set -u
ROOT="$(cd "$(dirname "$0")/../../GMBasic" && pwd)"; cd "$ROOT"

MODE=CORE; RUN_SAMPLES=0; ALL_SAMPLES=0; DRY=0; LIST=0; BUILD=0; ONLY=""
while [ $# -gt 0 ]; do case "$1" in
  --full)    MODE=FULL; RUN_SAMPLES=1; ALL_SAMPLES=1;;
  --samples) RUN_SAMPLES=1;;
  --list)    LIST=1;;
  -n|--dry-run) DRY=1;;
  --build)   BUILD=1;;
  --only)    shift; ONLY="${1:-}";;
  -h|--help) sed -n '2,25p' "$0"; exit 0;;
  *) echo "opzione ignota: $1 (usa -h)"; exit 2;;
esac; shift; done

# ── Filtri miniTest (sottostringhe di path, match ANY) = superficie LLVM/LLVM-runtime ──────────
CORE_FILTERS=(
  "llm/"                 # model compiler / surgery / IR / policy / quant / tensor
  "train/"               # training (SFT/LoRA/DPO/GRPO/distill)
  "ai/distill_"          # distill (ai)
  "ai/ai_model_"         # AI MODEL convert/quantize/inspect
  "ai/ai_llm_"           # AI-LLM
  "ai/llm_model_"        # streaming/load/unload/cache/prefetch
  "ai/llm_streamstats"   # streamstats
  "ai/memory_kv"         # KV memory
  "ai/ai_session_kv"     # KV sessione
  # §4.10 SEMAPRAX — GMB Semantic Graph (lato programma), fold a compile/link-time.
  "ai/ai_symbol"
  "ai/ai_callmap"
  "ai/ai_impact"
  "ai/ai_repomap"
  "ai/ai_findsymbol"
  "ai/ai_fileoutline"
  "ai/ai_revision"
  "ai/ai_checkrev"
  "ai/ai_patch"
  "llm/llm_inspect"
  "llm/llm_verify"
  "llm/llm_commit"
  "llm/llm_callmap_self"
)
FULL_FILTERS=("ai/")     # --full: tutto il dominio ai/

# ── Sample AiLLM rappresentativi (default) — il resto con --full ───────────────────────────────
SAMPLE_CORE=(
  samples/AiLLM/ModelSurgeryPrune.bas
  samples/AiLLM/ModelCompilerCompress.bas
  samples/AiLLM/ModelCompilerQuantAuto.bas
  samples/AiLLM/TrainCommandsTour.bas
  samples/AiLLM/DistillPipeline.bas
  samples/AiLLM/ModelSurgeryQualityGate.bas
)
SAMPLES=()          # valorizzato solo con --samples/--full

have() { [ -x "$1" ]; }

say() { echo "$@"; }

# ── Runner: parity (IL/PCode/VMC) ──────────────────────────────────────────────────────────────
run_parity() {
  local filters=("$@")
  if [ "$LIST" = 1 ] || [ "$DRY" = 1 ]; then say "  [dry] parity ${filters[*]}"; return 0; fi
  say "── parity IL/PCode/VMC :: ${filters[*]}"
  if [ "$BUILD" = 1 ]; then
    GMB_MINITEST_BUILD=1 tools/test_minitests_parity "${filters[@]}"
  else
    tools/test_minitests_parity "${filters[@]}"
  fi
}

# ── Runner: C99Native (single-substring -> loop) ────────────────────────────────────────────────
run_c99native() {
  local filters=("$@") f rc=0
  if [ "$LIST" = 1 ] || [ "$DRY" = 1 ]; then say "  [dry] c99native ${filters[*]}"; return 0; fi
  for f in "${filters[@]}"; do
    say "── C99Native :: $f"
    GMB_MINI_ONLY="$f" tools/sh/test_minitests_c99native || rc=1
  done
  return $rc
}


# ── Runner: sample AiLLM via parity4 (4 gambe) ──────────────────────────────────────────────────
# I sample vivono in `samples/...` alla root del repo GMBasic e in `shared/samples/...` nella
# root del MONOREPO (GMBasic e GMBasicSharp sono sibling di shared/). Si prova il percorso come
# dato e, se assente, sotto `../shared`: cosi' il gate funziona in entrambi i layout.
resolve_sample() {
  [ -f "$1" ] && { printf '%s\n' "$1"; return 0; }
  [ -f "../shared/$1" ] && { printf '%s\n' "../shared/$1"; return 0; }
  return 1
}

run_samples() {
  local files=("$@") f rc=0
  if [ "$LIST" = 1 ]; then printf '  [list] parity4 %s\n' "${files[@]}"; return 0; fi
  for f in "${files[@]}"; do
    f=$(resolve_sample "$f") || { say "  SKIP (assente): $f"; continue; }
    if [ "$DRY" = 1 ]; then say "  [dry] parity4 $f"; continue; fi
    say "── sample :: $f"
    tools/sh/parity4 "$f" --timeout 300 || rc=1
  done
  return $rc
}

# ── Selezione filtri/sample ────────────────────────────────────────────────────────────────────
if [ "$MODE" = FULL ]; then FILTERS=("${CORE_FILTERS[@]}" "${FULL_FILTERS[@]}"); else FILTERS=("${CORE_FILTERS[@]}"); fi
if [ -n "$ONLY" ]; then FILTERS=("$ONLY"); RUN_SAMPLES=0; ALL_SAMPLES=0; fi
if [ "$RUN_SAMPLES" = 1 ]; then
  if [ "$ALL_SAMPLES" = 1 ]; then
    SAMPLES=()
    while IFS= read -r s; do SAMPLES+=("$s"); done < <(ls samples/AiLLM/*.bas 2>/dev/null | sort)
  else
    SAMPLES=("${SAMPLE_CORE[@]}")
  fi
fi

say "════════════════════════════════════════════════════════════════════════════"
say " LLM/LLVM CROSS-DOMAIN GATE — mode=$MODE samples=$RUN_SAMPLES (heavy/interactive esclusi)"
say " rostro: LLM/TRAIN/TORCH/AI non devono rompere il compilatore/surgery/IR LLVM"
say "════════════════════════════════════════════════════════════════════════════"

if [ "$LIST" = 1 ]; then
  say "FILTRI miniTest: ${FILTERS[*]}"
  [ "$RUN_SAMPLES" = 1 ] && { say "SAMPLE (${#SAMPLES[@]}):"; printf '  %s\n' "${SAMPLES[@]}"; }
  say "Gambe: parity(IL/PCode/VMC) + C99Native (4/4 backend)"
  exit 0
fi

declare -a BLOCK; declare -a EST
block() { BLOCK+=("$1"); EST+=("$2"); }

# 1) parity IL/PCode/VMC
if run_parity "${FILTERS[@]}"; then block "miniTest IL/PCode/VMC" PASS; else block "miniTest IL/PCode/VMC" FAIL; fi

# 2) C99Native
if run_c99native "${FILTERS[@]}"; then block "miniTest C99Native" PASS; else block "miniTest C99Native" FAIL; fi


# 4) sample AiLLM (4 gambe)
if [ "$RUN_SAMPLES" = 1 ]; then
  if run_samples "${SAMPLES[@]}"; then block "sample AiLLM (parity4)" PASS; else block "sample AiLLM (parity4)" FAIL; fi
fi

say "────────────────────────────────────────────────────────────────────────────"
say " RIEPILOGO GATE"
reject=0
for i in "${!BLOCK[@]}"; do
  say "   ${EST[$i]}  ${BLOCK[$i]}"
  [ "${EST[$i]}" = FAIL ] && reject=1
done
say "────────────────────────────────────────────────────────────────────────────"
if [ "$reject" = 0 ]; then say " VERDETTO: ACCEPT (nessuna regressione rilevata)"; exit 0
else say " VERDETTO: REJECT (vedi blocchi FAIL sopra)"; exit 1; fi
