#!/usr/bin/env bash
# tools/sh/notebook_smoke.sh — smoke test del notebook server C (`gmbnotebook`).
#
#   tools/sh/notebook_smoke.sh [porta]
#
# Avvia Notebook/build/gmbnotebook su 127.0.0.1, esegue le celle, verifica stato e /vars,
# resetta, spegne. Exit 0 = tutto ok. Non è una suite: tocca un solo processo locale.
set -u

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
port=${1:-8049}
bin="$root/Notebook/build/gmbnotebook"
log=$(mktemp /tmp/gmb_notebook_smoke.XXXXXX)

[ -x "$bin" ] || { echo "?gmbnotebook non costruito: $bin" >&2; echo "  cmake -S Notebook -B Notebook/build && cmake --build Notebook/build --target gmbnotebook" >&2; exit 2; }

"$bin" --port "$port" >"$log" 2>&1 &
pid=$!
cleanup() { kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null; }
trap cleanup EXIT

# attesa health (max ~5s)
ok=0
for _ in $(seq 1 50); do
    if curl -s --max-time 1 "127.0.0.1:$port/health" | grep -q '"ok":true'; then ok=1; break; fi
    sleep 0.1
done
[ "$ok" = 1 ] || { echo "?server non risponde su 127.0.0.1:$port" >&2; tail -5 "$log" >&2; exit 1; }

fails=0
check() { # $1 = json, $2 = pattern, $3 = nome
    if printf '%s' "$1" | grep -qE "$2"; then echo "PASS  $3"; else echo "FAIL  $3: $1"; fails=$((fails+1)); fi
}

r=$(curl -s --max-time 5 -X POST "127.0.0.1:$port/execute" -d '{"cellId":"1","source":"x = 6 * 7"}')
check "$r" '"ok":true.*"value":42' "assegnazione + value"

r=$(curl -s --max-time 5 -X POST "127.0.0.1:$port/execute" -d '{"cellId":"2","source":"PRINT \"x =\"; x"}')
check "$r" '"output":"x =  42 "' "PRINT con stato persistente"

r=$(curl -s --max-time 5 "127.0.0.1:$port/vars")
check "$r" '"name":"x".*"value":42' "/vars"

r=$(curl -s --max-time 5 -X POST "127.0.0.1:$port/execute" -d '{"cellId":"3","source":"FOR i = 1 TO 3\nPRINT i;\nNEXT"}')
check "$r" '"output":" 1  2  3 "' "FOR/NEXT"

r=$(curl -s --max-time 5 -X POST "127.0.0.1:$port/execute" -d '{"cellId":"4","source":"PRINT 1/0"}')
check "$r" '"ok":false.*division by zero' "errore riportato"

r=$(curl -s --max-time 5 "127.0.0.1:$port/cells")
check "$r" '"cells":\[.*"cellId":"1".*"cellId":"2"' "/cells (celle memorizzate)"

r=$(curl -s --max-time 5 "127.0.0.1:$port/order")
check "$r" '"order":\["1","2"' "/order"

r=$(curl -s --max-time 5 -X POST "127.0.0.1:$port/runall")
check "$r" '"results":\[.*"ok":true.*"ok":true' "/runall (riesegue in ordine)"

r=$(curl -s --max-time 5 -X POST "127.0.0.1:$port/execute" -d '{"cellId":"5","source":"%md # titolo"}')
check "$r" '"ok":true.*"output":"# titolo"' "cella %md (passthrough)"

r=$(curl -s --max-time 5 -X POST "127.0.0.1:$port/execute" -d '{"cellId":"6","source":"%sql SELECT 1"}')
check "$r" '"ok":false.*non supportata' "cella %sql dichiarata non supportata"

r=$(curl -s --max-time 5 -X POST "127.0.0.1:$port/ai" -d '{}')
check "$r" '"ok":false.*non supportato' "/ai dichiarato non supportato"

r=$(curl -s --max-time 5 -X POST "127.0.0.1:$port/reset")
check "$r" '"ok":true' "/reset"

r=$(curl -s --max-time 5 "127.0.0.1:$port/vars")
check "$r" '"vars":\[\]' "/vars dopo reset"

r=$(curl -s --max-time 5 -X POST "127.0.0.1:$port/shutdown")
check "$r" '"ok":true' "/shutdown"

if [ "$fails" -eq 0 ]; then echo "== NOTEBOOK OK =="; exit 0; fi
echo "== $fails verifiche fallite (log: $log) =="
exit 1
