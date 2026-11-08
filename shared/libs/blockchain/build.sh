#!/usr/bin/env bash
# build.sh — copia fisica della lib `blockchain` dentro shared/libs.
#
# blockchain e' distribuita come PACCHETTO (dipendenza esterna OpenSSL 3): la build
# autoritativa e' quella del pacchetto, e questa copia non ne tiene una seconda.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../.." && pwd)
VER=$(ls "$ROOT/packages/blockchain" | sort -V | tail -1)
[ -n "$VER" ] || { echo "packages/blockchain: nessuna versione"; exit 1; }
exec bash "$ROOT/packages/blockchain/$VER/build.sh"
