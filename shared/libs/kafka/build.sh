#!/usr/bin/env bash
# build.sh — compila `libgmbkafka` (contratto unico: `tools/sh/gmb_lib.sh`).
# Output: shared/libs/kafka/libgmbkafka.<ext>  ·  export .inc in shared/include/ (header C in include/).
set -eu
MONO="$(cd "$(dirname "$0")/../../.." && pwd)"
exec bash "$MONO/tools/sh/gmb_lib.sh" build kafka
