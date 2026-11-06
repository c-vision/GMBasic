#!/usr/bin/env bash
# build.sh — compila `libgmbwsp` (contratto unico: tools/sh/gmb_lib.sh).
# Namespace BASIC: WSP (shared/include/wsp.inc). I sorgenti llmwsp_*.c/.h sono VENDUTI da GMBasic/gmbllm (sede
# canonica: lì), copiati qui prima della build; il binding piatto è src/wsp_ffi.c.
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
SRC="$MONO/GMBasic/gmbllm"
DST="$MONO/shared/libs/wsp"
for f in store doc mat linalg analyze fixture overlay derive verify agent; do
  cp "$SRC/src/llmwsp_$f.c" "$DST/src/"
  cp "$SRC/include/llmwsp_$f.h" "$DST/include/"
done
bash "$MONO/tools/sh/gmb_lib.sh" build wsp
