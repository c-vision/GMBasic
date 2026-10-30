#!/usr/bin/env bash
# tools/sh/msg_broker.sh — avvia/ferma il broker pub/sub di test (tools/c/msg_broker_mock.c).
#
# Serve ai MiniTest del trasporto tcp (es. messaging/message_tcp_parity.bas, broker su :9215).
# Da quando il test verifica una consegna CROSS-CONNESSIONE (pubblica su un bus, sottoscrive
# su un altro), il broker è un prerequisito REALE: senza di lui il test fallisce con
# "timeout" invece di passare consegnando in-process (era il falso verde del fallback locale).
#
# Uso:
#   tools/sh/msg_broker.sh start [porta] [retry_ms] [max_attempts]   (default 9215 300 3)
#   tools/sh/msg_broker.sh stop  [porta]
#   tools/sh/msg_broker.sh status [porta]
#   tools/sh/msg_broker.sh ensure [porta]                  → stampa `started` | `already` | `failed`
#   tools/sh/msg_broker.sh release [porta]                 → ferma SOLO ciò che ha avviato `ensure`
#   tools/sh/msg_broker.sh ports [filtro|file ...]         → porte richieste dai MiniTest selezionati
#
# `start` compila il mock al volo (cc), lo lascia in background e attende che la porta
# risponda; è idempotente: se la porta è già servita non fa nulla.
#
# `ports` + `ensure`/`release` esistono per i GATE (test_minitests_parity.sh,
# test_minitests_c99native, parity4): il broker è un prerequisito del test, non un gesto
# manuale. Prima andava avviato a mano: se non c'era (o era morto a metà run) i MiniTest tcp
# fallivano con "timeout" e restava ambiguo se la consegna verificata fosse quella
# cross-connessione o un altro percorso. `ports` è AUTODESCRITTIVO — legge gli `.bas`
# selezionati e prende le porte dai loro `tcp://` loopback: nessuna lista da tenere allineata.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/tools/c/msg_broker_mock.c"
BIN="${TMPDIR:-/tmp}/msg_broker_mock"
PORT="${2:-9215}"
RETRY="${3:-300}"
ATTEMPTS="${4:-3}"
PIDFILE="${TMPDIR:-/tmp}/msg_broker_$PORT.pid"

broker_up() { nc -z 127.0.0.1 "$1" 2>/dev/null; }

# Porte richieste dai MiniTest: `tcp://127.0.0.1:<porta>` / `tcp://localhost:<porta>`.
# Gli argomenti sono FILTRI (sottostringa del path, come GMB_MINI_ONLY) oppure FILE esistenti;
# senza argomenti si guardano tutti i MiniTest.
broker_ports() {
  local root="${GMB_MINI_ROOT:-}"
  if [ -z "$root" ]; then
    for cand in "$ROOT/MiniTests" "$ROOT/shared/minitests" "$ROOT/../shared/minitests"; do
      [ -d "$cand" ] && { root="$cand"; break; }
    done
  fi
  [ -n "$root" ] || return 1
  {
    if [ $# -eq 0 ]; then
      find "$root" -name '*.bas' -print 2>/dev/null
    else
      for a in "$@"; do
        if [ -f "$a" ]; then printf '%s\n' "$a"; continue; fi
        find "$root" -name '*.bas' -print 2>/dev/null | while IFS= read -r f; do
          case "$f" in *"$a"*) printf '%s\n' "$f";; esac
        done
      done
    fi
  } | sort -u | while IFS= read -r f; do
      grep -oE 'tcp://(127\.0\.0\.1|localhost):[0-9]+' "$f" 2>/dev/null | sed 's|.*:||'
    done | sort -n | uniq
}

case "${1:-}" in
  start|ensure)
    if broker_up "$PORT"; then
      [ "${1:-}" = "ensure" ] && echo already || echo "broker già attivo su :$PORT"
      exit 0
    fi
    [ -f "$SRC" ] || { echo "sorgente non trovato: $SRC" >&2; [ "${1:-}" = "ensure" ] && echo failed; exit 1; }
    cc -O2 -o "$BIN" "$SRC" || { echo "compilazione del broker fallita" >&2; [ "${1:-}" = "ensure" ] && echo failed; exit 1; }
    # nohup: il broker deve sopravvivere alla shell che lo ha avviato (altrimenti muore col
    # terminale e il gate successivo trova "timeout").
    nohup "$BIN" "$PORT" "$RETRY" "$ATTEMPTS" >"${TMPDIR:-/tmp}/msg_broker_$PORT.log" 2>&1 &
    # PID registrato: `release`/`stop` fermano QUESTO processo, non "chiunque si chiami così"
    # (un pkill -f colpisce anche la shell di un'altra sessione che nomina la stessa porta).
    echo $! >"$PIDFILE"
    for _ in 1 2 3 4 5 6 7 8 9 10; do
      sleep 0.2
      if broker_up "$PORT"; then
        if [ "${1:-}" = "ensure" ]; then echo started
        else echo "broker attivo su :$PORT (retry=${RETRY}ms max_attempts=$ATTEMPTS)"; fi
        exit 0
      fi
    done
    echo "broker non raggiungibile su :$PORT (vedi ${TMPDIR:-/tmp}/msg_broker_$PORT.log)" >&2
    [ "${1:-}" = "ensure" ] && echo failed
    exit 1
    ;;
  stop|release)
    pid=""
    [ -f "$PIDFILE" ] && pid="$(cat "$PIDFILE" 2>/dev/null)"
    if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
      kill "$pid" 2>/dev/null
      sleep 0.2
      kill -0 "$pid" 2>/dev/null && kill -9 "$pid" 2>/dev/null
      rm -f "$PIDFILE"
      echo "broker fermato (:$PORT, pid $pid)"
    else
      rm -f "$PIDFILE"
      echo "nessun broker avviato da questo script su :$PORT"
    fi
    exit 0
    ;;
  status)
    if broker_up "$PORT"; then echo "broker attivo su :$PORT"; else echo "broker NON attivo su :$PORT"; fi
    ;;
  ports)
    shift
    broker_ports "$@"
    ;;
  *)
    echo "uso: $0 start|stop|status|ensure|release [porta] [retry_ms] [max_attempts] | ports [filtro|file ...]" >&2
    exit 2
    ;;
esac
