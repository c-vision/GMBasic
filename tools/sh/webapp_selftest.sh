#!/bin/zsh
# webapp_selftest.sh — verifica END-TO-END della WEBAPP (Agents Operating Center)
# ============================================================================
# Avvia la app (se non è già attiva), esercita pagine, statici, API proprie e
# proxy, poi ripulisce ciò che ha creato. Esce 1 al primo gruppo di fallimenti.
#
# Uso:  tools/sh/webapp_selftest.sh            (avvia/ferma da sé)
#       tools/sh/webapp_selftest.sh --keep     (se era già attiva, la lascia attiva)
#
# Non è un test massivo: un solo processo, ~25 richieste, ~30 secondi.
# ============================================================================
set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
UP="$ROOT/tools/sh/webapp_up.sh"
CFG="$ROOT/shared/webapp/.webapp.toml"

port_of() {
  local key="$1" def="$2" v=""
  if [ -f "$CFG" ]; then
    v="$(awk -F= -v k="$key" '
      { line=$0; sub(/#.*/,"",line); gsub(/[ \t]/,"",line);
        if (tolower(line) ~ "^" tolower(k) "=") { sub(/^[^=]*=/,"",line); gsub(/"/,"",line); print line; exit } }' "$CFG" 2>/dev/null)"
  fi
  [ -n "$v" ] && echo "$v" || echo "$def"
}

DASH_PORT="$(port_of dashboardPort 8090)"
API_PORT="$(port_of apiPort 8765)"
B="http://127.0.0.1:${DASH_PORT}"
PASS=0; FAIL=0

ok()   { PASS=$((PASS+1)); printf '  \033[32mPASS\033[0m %s\n' "$1"; }
bad()  { FAIL=$((FAIL+1)); printf '  \033[31mFAIL\033[0m %s\n' "$1"; [ -n "${2:-}" ] && printf '       %s\n' "$2"; }

# code METHOD URL [BODY] → http code
code() {
  local m="$1" u="$2" d="${3:-}"
  if [ -n "$d" ]; then
    curl -s -m 10 -o /dev/null -w '%{http_code}' -X "$m" -H 'Content-Type: application/json' -d "$d" "$u"
  else
    curl -s -m 10 -o /dev/null -w '%{http_code}' -X "$m" "$u"
  fi
}
body() { curl -s -m 10 "$1"; }

expect_code() {
  local want="$1" got="$2" label="$3"
  if [ "$got" = "$want" ]; then ok "$label ($got)"; else bad "$label" "atteso $want, ottenuto $got"; fi
}

json_get() { # json_get <json> <python-expr su d>
  if command -v python3 >/dev/null 2>&1; then
    echo "$1" | python3 -c "import json,sys; d=json.load(sys.stdin); print($2)" 2>/dev/null
  fi
}

cleanup() {
  # ripulisce SOLO ciò che ha creato questo test
  curl -s -m 5 -X DELETE "$B/api/agents/ST_Agent" >/dev/null 2>&1
  curl -s -m 5 -X DELETE "$B/api/agents/ST_Persist" >/dev/null 2>&1
  curl -s -m 5 -X DELETE "$B/api/teams/ST_Team" >/dev/null 2>&1
  curl -s -m 5 -X DELETE "$B/api/workflows/ST_Wf" >/dev/null 2>&1
  curl -s -m 5 -X DELETE "$B/api/memory?key=st_key" >/dev/null 2>&1
}
trap cleanup EXIT

WAS_RUNNING=0
if curl -s -m 2 -o /dev/null "$B/api/health" 2>/dev/null; then
  WAS_RUNNING=1
  echo "webapp già attiva su :$DASH_PORT — uso quella"
else
  echo "avvio la webapp..."
  "$UP" start >/dev/null || { echo "impossibile avviare la webapp"; exit 1; }
fi

echo ""
echo "== pagine e statici =="
expect_code 200 "$(code GET "$B/index.bsp")"  "index.bsp"
expect_code 200 "$(code GET "$B/canvas.bsp")" "canvas.bsp"
expect_code 200 "$(code GET "$B/wasm.bsp")"   "wasm.bsp"
expect_code 200 "$(code GET "$B/static/canvas.js")" "static/canvas.js"
expect_code 200 "$(code GET "$B/static/wasm.js")"   "static/wasm.js"
HTML="$(body "$B/index.bsp")"
case "$HTML" in
  *"Agents Operating Center"*) ok "index.bsp contiene il titolo";;
  *) bad "index.bsp senza titolo";;
esac
case "$HTML" in
  *"function refreshStatus"*) ok "index.bsp contiene il JS della UI";;
  *) bad "index.bsp senza JS";;
esac

echo ""
echo "== API proprie =="
expect_code 200 "$(code GET "$B/api/health")" "GET /api/health"
ST="$(body "$B/api/status")"
if [ -n "$ST" ]; then ok "GET /api/status ($(json_get "$ST" 'd.get("version")'))"; else bad "GET /api/status"; fi
[ "$(json_get "$ST" 'd.get("dashboardPort")')" = "$DASH_PORT" ] && ok "status: dashboardPort=$DASH_PORT" || bad "status: dashboardPort errato"
[ "$(json_get "$ST" 'd.get("apiReachable")')" = "True" ] && ok "status: apiReachable" || bad "status: api non raggiungibile"
[ -n "$(json_get "$ST" 'd.get("teams")')" ] && ok "status: teams=$(json_get "$ST" 'd.get("teams")')" || bad "status: manca teams"

echo ""
echo "== agenti (CRUD) =="
expect_code 201 "$(code POST "$B/api/agents" '{"name":"ST_Agent","systemPrompt":"test","permission":"Sandbox"}')" "POST /api/agents"
expect_code 409 "$(code POST "$B/api/agents" '{"name":"ST_Agent"}')" "POST duplicato → 409"
LIST="$(body "$B/api/agents")"
case "$LIST" in *'"name":"ST_Agent"'*) ok "l'agente compare in /api/agents";; *) bad "agente non presente in /api/agents";; esac
expect_code 200 "$(code PATCH "$B/api/agents/ST_Agent" '{"permission":"YOLO","temperature":"0.2"}')" "PATCH /api/agents/ST_Agent"
ASK="$(curl -s -m 25 -X POST -H 'Content-Type: application/json' -d '{"prompt":"ping"}' "$B/api/agents/ST_Agent/ask")"
case "$ASK" in *'"answer"'*) ok "POST /api/agents/ST_Agent/ask";; *) bad "ask senza answer" "$ASK";; esac
expect_code 200 "$(code DELETE "$B/api/agents/ST_Agent")" "DELETE /api/agents/ST_Agent"

echo ""
echo "== workflow =="
expect_code 200 "$(code POST "$B/api/workflows" '{"name":"ST_Wf"}')" "POST /api/workflows"
expect_code 200 "$(code POST "$B/api/workflows/ST_Wf/steps" '{"name":"s1","action":"PRINT 1"}')" "POST steps"
EXEC="$(curl -s -m 15 -X POST -H 'Content-Type: application/json' -d '{}' "$B/api/workflows/ST_Wf/execute")"
case "$EXEC" in *'"ok":true'*) ok "POST execute (ok:true)";; *) bad "execute non ok" "$EXEC";; esac
case "$(body "$B/api/workflows")" in *'"name":"ST_Wf"'*) ok "workflow elencato";; *) bad "workflow non elencato";; esac
expect_code 200 "$(code DELETE "$B/api/workflows/ST_Wf")" "DELETE /api/workflows/ST_Wf"

echo ""
echo "== persistenza agenti (bin/agents) =="
DATA="$ROOT/shared/webapp/bin"
expect_code 201 "$(code POST "$B/api/agents" '{"name":"ST_Persist","systemPrompt":"persist","permission":"Sandbox"}')" "POST /api/agents ST_Persist"
if [ -f "$DATA/agents/ST_Persist.agent.json" ]; then ok "file agente creato in bin/agents"; else bad "file agente non creato" "$DATA/agents/ST_Persist.agent.json"; fi
expect_code 200 "$(code DELETE "$B/api/agents/ST_Persist")" "DELETE /api/agents/ST_Persist"
if [ ! -f "$DATA/agents/ST_Persist.agent.json" ]; then ok "file agente rimosso"; else bad "file agente ancora presente"; fi

echo ""
echo "== team (CRUD + persistenza bin/teams) =="
expect_code 201 "$(code POST "$B/api/teams" '{"name":"ST_Team","strategy":"RoundRobin"}')" "POST /api/teams"
case "$(body "$B/api/teams")" in *'"name":"ST_Team"'*) ok "team elencato";; *) bad "team non elencato";; esac
expect_code 409 "$(code POST "$B/api/teams" '{"name":"ST_Team"}')" "POST team duplicato → 409"
expect_code 200 "$(code POST "$B/api/teams/ST_Team/agents" '{"agent":"Demo","role":"Worker"}')" "POST team/{n}/agents"
TEAMJ="$(body "$B/api/teams/ST_Team")"
case "$TEAMJ" in *Demo*) ok "membro Demo presente nel team";; *) bad "membro non presente" "$TEAMJ";; esac
if [ -f "$DATA/teams/ST_Team.team.json" ]; then ok "file team creato in bin/teams"; else bad "file team non creato"; fi
expect_code 200 "$(code DELETE "$B/api/teams/ST_Team")" "DELETE /api/teams/ST_Team"
if [ ! -f "$DATA/teams/ST_Team.team.json" ]; then ok "file team rimosso"; else bad "file team ancora presente"; fi

echo ""
echo "== memory =="
expect_code 200 "$(code POST "$B/api/memory" '{"key":"st_key","value":"st_value"}')" "POST /api/memory"
MEM="$(body "$B/api/memory?key=st_key")"
case "$MEM" in *'"value":"st_value"'*) ok "GET /api/memory?key=st_key (query inoltrata)";; *) bad "memory: valore assente" "$MEM";; esac
case "$(body "$B/api/memory")" in *'"keys"'*) ok "GET /api/memory (lista chiavi)";; *) bad "memory: lista assente";; esac
expect_code 200 "$(code DELETE "$B/api/memory?key=st_key")" "DELETE /api/memory?key=st_key"

echo ""
echo "== brain / enrich =="
expect_code 200 "$(code GET "$B/api/brain/list?limit=2")" "GET /api/brain/list"
expect_code 200 "$(code GET "$B/api/brain/search?query=test&topK=1")" "GET /api/brain/search"
ENRICH="$(code POST "$B/api/enrich" '{"text":"ciao mondo","mode":"SUMMARIZATION"}')"
if [ "$ENRICH" = "200" ] || [ "$ENRICH" = "500" ]; then ok "POST /api/enrich raggiungibile ($ENRICH)"; else bad "enrich" "http $ENRICH"; fi

echo ""
echo "== Streaming SSE (chunked) /api/events/stream =="
SSE_HDR="$(curl -s -N -m 3 -D - -o /dev/null "$B/api/events/stream" 2>/dev/null | tr -d '\r')"
case "$SSE_HDR" in
  *"text/event-stream"*) ok "Content-Type: text/event-stream";;
  *) bad "SSE content-type" "$(printf '%s' "$SSE_HDR" | head -1)";;
esac
case "$SSE_HDR" in
  *"Transfer-Encoding: chunked"*) ok "Transfer-Encoding: chunked";;
  *) bad "SSE chunked" "nessun Transfer-Encoding: chunked";;
esac
# evento LIVE: apre lo stream, crea un agente, l'evento deve arrivare sul canale aperto
SSE_OUT="$(mktemp)"
( curl -s -N -m 6 "$B/api/events/stream" > "$SSE_OUT" 2>/dev/null ) &
SSE_PID=$!
sleep 1
code POST "$B/api/agents" '{"name":"SseProbe"}' >/dev/null 2>&1
sleep 3
if grep -q 'SseProbe' "$SSE_OUT" 2>/dev/null; then
  ok "evento live consegnato sullo stream aperto"
else
  bad "SSE live" "nessun evento SseProbe ricevuto"
fi
wait $SSE_PID 2>/dev/null
rm -f "$SSE_OUT"
code DELETE "$B/api/agents/SseProbe" >/dev/null 2>&1

echo ""
echo "== WebSocket /ws/canvas =="
WS="$(curl -s -i -N -m 4 -H 'Connection: Upgrade' -H 'Upgrade: websocket' \
      -H 'Sec-WebSocket-Version: 13' -H 'Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==' \
      "$B/ws/canvas" 2>/dev/null | head -1)"
case "$WS" in
  *"101"*) ok "handshake 101";;
  *) bad "handshake WebSocket" "$WS";;
esac

cleanup
if [ "$WAS_RUNNING" = "0" ] && [ "${1:-}" != "--keep" ]; then
  echo ""
  "$UP" stop >/dev/null && echo "webapp fermata (era stata avviata dal test)"
fi

echo ""
echo "──────────────────────────────────────────"
echo "  PASS: $PASS   FAIL: $FAIL"
[ "$FAIL" = "0" ] || exit 1
echo "  selftest OK"
