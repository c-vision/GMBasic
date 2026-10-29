#!/bin/zsh
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

# rebuild_ai_native.sh — ricostruisce la catena AI nativa NELL'ORDINE giusto (anti-stale).
# Dopo aver toccato sorgenti AI (gmbllm/, gmb_runtime_c/, LLMCodec), i consumer linkano
# by-path libgmb_runtime.a/libgmbllm.a: vanno ricostruiti TUTTI, in quest'ordine.
set -e
ROOT="$(cd "$(dirname "$0")/../../GMBasic" && pwd)"
echo "== 1) gmb_runtime (+ gmbllm) =="
cmake --build "$ROOT/native/gmb_runtime_c/build" --target gmb_runtime -j 6 >/dev/null
echo "== 2) gmb_ai_llm =="
cmake --build "$ROOT/native/gmb_runtime_c/build" --target gmb_ai_llm -j 6 >/dev/null
echo "== 3) consumer VM/C (gmb_vm_host) =="
cmake --build "$ROOT/native/gmb_vm_c/build" --target gmb_vm_host -j 6 >/dev/null
for p in gmb_agent_runtime gmb_llm_server gmb_torch; do
  if [ -d "$ROOT/native/$p/build" ]; then
    echo "== consumer $p =="
    cmake --build "$ROOT/native/$p/build" -j 6 >/dev/null 2>&1 || echo "  (skip $p)"
  fi
done
echo "== OK: catena AI nativa ricostruita =="
