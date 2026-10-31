#!/usr/bin/env bash
# build.sh — compila `libgmbnats` (contratto unico: `tools/sh/gmb_lib.sh`).
# Output: shared/libs/nats/libgmbnats.<ext>  ·  export .inc e header pubblici in shared/include/.
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
exec bash "$MONO/tools/sh/gmb_lib.sh" build nats
