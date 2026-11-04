#!/usr/bin/env bash
# build.sh — compila `libgmbflow` (contratto unico: tools/sh/gmb_lib.sh).
# Namespace BASIC: GRAPH (flow.inc). Motore: gmbortools (gor_flow/gor_flow2/gor_graph).
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
bash "$MONO/tools/sh/gmb_lib.sh" build flow
