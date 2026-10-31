#!/usr/bin/env bash
# build.sh — compila `libgmbmqtt` (contratto unico: `tools/sh/gmb_lib.sh`).
# Output: shared/libs/mqtt/libgmbmqtt.<ext>  ·  export .inc e header pubblici in shared/include/.
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
exec bash "$MONO/tools/sh/gmb_lib.sh" build mqtt
