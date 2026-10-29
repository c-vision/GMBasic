#!/bin/bash
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

# install_mlx_dylib.sh — installa ATOMICAMENTE le dylib native nel bench.
#
# PERCHE' (bug (open) - MLX-Prefill-SIGKILL-Sopra-65536, root cause 2026-09-18):
# `cp -p src dst` su un dst ESISTENTE lo apre con O_TRUNC e lo riscrive IN PLACE (stesso inode).
# Se un altro processo ha quella dylib MAPPATA (es. un bench in corso), macOS invalida la firma
# ad-hoc della pagina eseguibile e uccide il processo con
#   SIGKILL "Code Signature Invalid" / CODESIGNING "Invalid Page"
# (stack in dyld dlopen). Sintomo: run lunghi (ctx>64k) muoiono con EXIT=137 e ZERO output.
#
# FIX: copiare su un file temporaneo e poi RENAME (mv) sull'dst. Il rename sostituisce il PATH con
# un NUOVO inode: i processi che hanno ancora mappato il vecchio inode restano validi (nessuna
# pagina modificata), i nuovi run aprono il nuovo file. Nessuna modifica in-place = nessun kill.
#
# Uso:
#   tools/sh/install_mlx_dylib.sh [src-dylib] [dst-dylib]
# Default: $LLMENV/LLMEngine/mlx/build-gmbasic/libllm_ai_mlx.dylib ->
#          test_mlx_bench/bin/Release/net10.0/libgmb_ai_mlx.dylib
#
# Nota: NON installare con `cp` diretto nel working tree condiviso (piu' AI + bench in corso).

set -euo pipefail
cd "$(dirname "$0")/../../GMBasic" || exit 2

LLMENV="${LLMENV:-$HOME/Documents/Projects/LLMEnv}"
SRC="${1:-$LLMENV/LLMEngine/mlx/build-gmbasic/libllm_ai_mlx.dylib}"
DST="${2:-test_mlx_bench/bin/Release/net10.0/libgmb_ai_mlx.dylib}"

[ -f "$SRC" ] || { echo "ERRORE: manca $SRC"; exit 2; }
dstdir="$(dirname "$DST")"; [ -d "$dstdir" ] || { echo "ERRORE: manca la dir $dstdir"; exit 2; }

tmp="${dstdir}/.$(basename "$DST").new.$$"
trap 'rm -f "$tmp"' EXIT
cp -p "$SRC" "$tmp"
mv -f "$tmp" "$DST"     # RENAME atomico: nuovo inode, nessuna pagina mappata modificata
trap - EXIT

echo "OK (atomico): $SRC -> $DST"
echo "   sha=$(shasum "$DST" | cut -c1-16)  mtime=$(stat -f '%Sm' "$DST")"
if codesign -v "$DST" 2>/dev/null; then echo "   codesign: valida"; else echo "   codesign: NON valida (ad-hoc?)"; fi