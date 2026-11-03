#!/bin/zsh
# webapp_up.sh — gestione del processo della WEBAPP (Agents Operating Center)
# ============================================================================
# Uso:
#   tools/sh/webapp_up.sh start [c|csharp]  # avvia in background (default: c)
#   tools/sh/webapp_up.sh stop              # ferma il processo avviato da start
#   tools/sh/webapp_up.sh restart [c|csharp]
#   tools/sh/webapp_up.sh status            # stato (pid, health, /api/status)
#   tools/sh/webapp_up.sh logs              # tail -f del log
#   tools/sh/webapp_up.sh url               # stampa gli URL
#
# DEFAULT = compilatore C (`gmb run --project shared/webapp`): è il mondo canonico,
# il processo è un singolo eseguibile nativo (runtime C, server HTTP + agent API).
# `csharp` resta disponibile come alternativa (`gmbcs run --engine pcode`).
# Porte/seed/refresh vengono da shared/webapp/.webapp.toml.
# PID e log stanno in ${TMPDIR} (mai nel repo).
# ============================================================================
set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
APP="$ROOT/shared/webapp/app.bas"
CFG="$ROOT/shared/webapp/.webapp.toml"
GMB="$ROOT/GMBasic/native/gmb_compiler_c/build/gmb"
REPL="$ROOT/GMBasicSharp/CompilerCli/bin/Debug/net10.0/gmbcs"
ENGINE="c"
PIDFILE="${TMPDIR:-/tmp}/gmb_webapp.pid"
LOGFILE="${TMPDIR:-/tmp}/gmb_webapp.log"

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
BASE="http://127.0.0.1:${DASH_PORT}"

pid_running() {
  [ -f "$PIDFILE" ] || return 1
  local pid; pid="$(cat "$PIDFILE" 2>/dev/null)"
  [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null
}

health_ok() {
  curl -s -m 2 -o /dev/null -w '%{http_code}' "$BASE/api/health" 2>/dev/null | grep -q '^200$'
}

wait_health() {
  local i=0
  while [ $i -lt 60 ]; do
    health_ok && return 0
    sleep 0.4
    i=$((i+1))
  done
  return 1
}

cmd_start() {
  if pid_running; then
    echo "già in esecuzione (pid $(cat "$PIDFILE")) → $BASE/index.bsp"
    return 0
  fi
  if [ "$ENGINE" = "csharp" ]; then
    if [ ! -x "$REPL" ]; then
      echo "errore: engine C# (gmbcs) non compilato: $REPL" >&2
      echo "  compila:  dotnet build GMBasicSharp/CompilerCli/CompilerCli.csproj" >&2
      return 1
    fi
  else
    if [ ! -x "$GMB" ]; then
      echo "errore: compilatore C non compilato: $GMB" >&2
      echo "  compila:  cmake --build GMBasic/native/gmb_compiler_c/build --target gmb_cli" >&2
      return 1
    fi
  fi
  if [ ! -f "$APP" ]; then echo "errore: app mancante: $APP" >&2; return 1; fi
  : > "$LOGFILE"
  if [ "$ENGINE" = "csharp" ]; then
    nohup "$REPL" run --engine pcode "$APP" >>"$LOGFILE" 2>&1 &
  else
    # `run`: build + esecuzione con CWD = cartella del progetto; l'eseguibile e gli
    # agenti persistiti stanno in shared/webapp/bin (mai versionato).
    nohup "$GMB" run --project "$ROOT/shared/webapp" --entry app.bas >>"$LOGFILE" 2>&1 &
  fi
  echo $! > "$PIDFILE"
  if wait_health; then
    echo "avviata (pid $(cat "$PIDFILE"))"
    cmd_url
  else
    echo "errore: la dashboard non risponde su $BASE" >&2
    echo "--- ultime righe del log ---" >&2
    tail -n 25 "$LOGFILE" >&2
    cmd_stop >/dev/null 2>&1
    return 1
  fi
}

cmd_stop() {
  if ! pid_running; then
    rm -f "$PIDFILE"
    echo "non in esecuzione"
    return 0
  fi
  local pid; pid="$(cat "$PIDFILE")"
  kill -TERM "$pid" 2>/dev/null
  local i=0
  while kill -0 "$pid" 2>/dev/null && [ $i -lt 40 ]; do sleep 0.25; i=$((i+1)); done
  if kill -0 "$pid" 2>/dev/null; then kill -KILL "$pid" 2>/dev/null; fi
  rm -f "$PIDFILE"
  echo "fermata (pid $pid)"
}

cmd_status() {
  if pid_running; then
    echo "processo : attivo (pid $(cat "$PIDFILE"))"
  else
    echo "processo : fermo"
  fi
  echo "dashboard: $BASE/index.bsp"
  echo "api      : http://127.0.0.1:$API_PORT/api/agents"
  if health_ok; then
    echo "health   : ok"
    local st; st="$(curl -s -m 3 "$BASE/api/status" 2>/dev/null)"
    if [ -n "$st" ]; then
      if command -v python3 >/dev/null 2>&1; then
        echo "$st" | python3 -c 'import json,sys
try:
    d=json.load(sys.stdin)
    print("runtime  : provider=%s model=%s agenti=%s eventi=%s uptime=%ss api=%s" % (
        d.get("activeProvider") or "-", d.get("model") or "-", d.get("agents"), d.get("events"),
        d.get("uptimeSec"), "ok" if d.get("apiReachable") else "giù"))
except Exception as e:
    print("status   : (json non leggibile: %s)" % e)'
      else
        echo "status   : $st"
      fi
    fi
  else
    echo "health   : NON risponde"
  fi
  [ -f "$LOGFILE" ] && echo "log      : $LOGFILE"
}

cmd_logs() { [ -f "$LOGFILE" ] || { echo "nessun log: $LOGFILE" >&2; return 1; }; tail -n 100 -f "$LOGFILE"; }

cmd_url() {
  echo "  dashboard: $BASE/index.bsp"
  echo "  canvas   : $BASE/canvas.bsp"
  echo "  wasm     : $BASE/wasm.bsp"
  echo "  api      : $BASE/api/agents  (proxy) / http://127.0.0.1:$API_PORT/api/agents (diretto)"
}

CMD="${1:-status}"
if [ "${2:-}" = "c" ] || [ "${2:-}" = "csharp" ]; then ENGINE="$2"; fi

case "$CMD" in
  start)   cmd_start ;;
  stop)    cmd_stop ;;
  restart) cmd_stop; cmd_start ;;
  status)  cmd_status ;;
  logs)    cmd_logs ;;
  url)     cmd_url ;;
  *) echo "uso: $0 start|stop|restart|status|logs|url [c|csharp]" >&2; exit 2 ;;
esac
