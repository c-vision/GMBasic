#!/usr/bin/env bash
# build.sh — compila `libgmbgraph` (contratto unico: tools/sh/gmb_lib.sh).
# Namespace BASIC: GRAPH (graph.inc). Motore: gmbortools (gor_graph/gor_graph2/gor_graph3)
# + shim ABI in src/graph_ffi.c. Sources copiati da gmbsuite/gmbortools (copia autonoma).
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
bash "$MONO/tools/sh/gmb_lib.sh" build graph
