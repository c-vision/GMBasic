#!/usr/bin/env bash
# build.sh — compila `libgmbneural` (contratto unico: tools/sh/gmb_lib.sh).
# Namespace BASIC: NEURAL (neural.inc). Slice ricorrente di gmbneural (RNN/LSTM/GRU/BiRNN
# + PBP) + nucleo interno (gn_core/act/opt) + gc_gemm/gc_array copiati da shared/libs/math.
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
bash "$MONO/tools/sh/gmb_lib.sh" build neural
