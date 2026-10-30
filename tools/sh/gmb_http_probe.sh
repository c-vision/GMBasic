#!/usr/bin/env bash
# tools/sh/gmb_http_probe.sh — sonda un sample SERVER HTTP su una o più gambe: avvia il
# runner in background, fa polling curl sull'endpoint finché il corpo contiene EXPECT,
# poi termina il runner. Serve perché i runner parity4/gate NON confrontano il corpo
# della risposta HTTP: senza sonda, un sample-server è "identico" su tutte le gambe
# anche se gli handler sono rotti.
#
# Uso: tools/sh/gmb_http_probe.sh <sample.bas> <expect> [url] [legs]
#   url  default http://127.0.0.1:8156/test
#   legs default il,pcode,vmc,c99n (emitter C# ritirato)
# CWD = root del repo. Nessuna variabile d'ambiente (parametri solo da argomenti).
# NB: le gambe girano SEQUENZIALI sulla stessa porta; un listener superstite di
#     un'altra sessione su quella porta produce un OK/FAIL inaffidabile → il tool
#     aspetta che la porta si liberi e avvisa.
set -u
export LC_ALL=C
[ $# -ge 2 ] || { echo "uso: $0 <sample.bas> <expect> [url] [legs]"; exit 2; }
FILE="$1"; EXPECT="$2"; URL="${3:-http://127.0.0.1:8156/test}"; LEGS="${4:-il,pcode,vmc,c99n}"
PORT=$(printf '%s' "$URL" | sed -E 's#.*://[^/]*:([0-9]+).*#\1#')
ROOT=$(cd "$(dirname "$0")/../../GMBasic" && pwd)
RUNNERS="$(cd "$(dirname "$0")/../.." && pwd)/tools/runners"   # harness C# (IL/PCode/VM-C)
cd "$ROOT"
DIR=$(mktemp -d); trap 'rm -rf "$DIR"' EXIT

wait_port_free() {
  local i
  for i in $(seq 1 20); do
    lsof -tiTCP:"$PORT" -sTCP:LISTEN >/dev/null 2>&1 || return 0
    sleep 0.5
  done
  echo "ATTENZIONE: porta $PORT ancora occupata da un processo esterno (non lo uccido): risultato inaffidabile"
}

run_leg() {
  local leg="$1" cmd
  case "$leg" in
    il)    cmd="dotnet "$RUNNERS/test_ilrun/bin/Debug/net10.0/test_ilrun.dll" --engine=il \"$FILE\"" ;;
    pcode) cmd="dotnet "$RUNNERS/test_ilrun/bin/Debug/net10.0/test_ilrun.dll" --engine=pcode \"$FILE\"" ;;
    vmc)   cmd="dotnet "$RUNNERS/test_vmrun/bin/Debug/net10.0/test_vmrun.dll" \"$FILE\"" ;;
    c99n)  cmd="bash tools/sh/gmb_c99native_run.sh \"$FILE\"" ;;

    *) echo "lega ignota: $leg"; return 2 ;;
  esac
  wait_port_free
  eval "$cmd" >"$DIR/$leg.log" 2>&1 &
  local pid=$! body="" i
  for i in $(seq 1 60); do
    sleep 0.5
    # max-time 15: gli handler GPM possono parcheggiare (SLEEP/canale) e rispondere dopo
    # secondi; un max-time corto produce un FAIL FALSO (rc 28) mentre il server è sano.
    body=$(curl -s --max-time 15 "$URL" 2>/dev/null || true)
    case "$body" in *"$EXPECT"*) break ;; esac
    kill -0 "$pid" 2>/dev/null || break
  done
  kill "$pid" 2>/dev/null; pkill -P "$pid" 2>/dev/null
  wait "$pid" 2>/dev/null
  local rc=$?
  if [ "$rc" -ne 0 ] && kill -0 "$pid" 2>/dev/null; then sleep 1; kill -9 "$pid" 2>/dev/null; fi
  case "$body" in
    *"$EXPECT"*) echo "leg=$leg OK body=[$body]" ;;
    *) echo "leg=$leg FAIL body=[$body]"; echo "  --- ultime righe log ---"; tail -6 "$DIR/$leg.log" | sed 's/^/  | /' ;;
  esac
}

IFS=',' read -ra LARR <<< "$LEGS"
for l in "${LARR[@]}"; do run_leg "$l"; done
