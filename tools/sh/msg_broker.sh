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

# ── LEASE PER PORTA (2026-11-14) ─────────────────────────────────────────────────────────────
# Un broker è un RECINTO CONDIVISO: due run parallele sullo stesso :9215 si scambiano i messaggi
# (l'una riceve le consegne dell'altra → `leaked`/`nodups`/`timeout` nei MiniTest messaging, che
# da soli passano). Il lease serializza l'USO della porta: chi lo tiene è l'unico che può
# avviare/usare quel broker, gli altri ASPETTANO che lo rilasci (nessuno "ruba lo slot").
# Proprietario = PID della RUN, passato dal runner come `$5`: NON `$PPID`, che dentro una
# sostituzione di comando è la subshell e morirebbe subito, facendo sembrare il lease orfano
# mentre la run sta ancora usando il broker.
LEASEDIR="${TMPDIR:-/tmp}/msg_broker_$PORT.lease"
OWNER="${5:-$PPID}"
LEASE_TICKS="${6:-6000}"        # 6000 × 0.1 s = 600 s di attesa massima, poi `busy`

lease_owner() { cat "$LEASEDIR/pid" 2>/dev/null; }

# lease_ancestor_owns: 0 se il lease è tenuto da un ANTENATO di chi chiama. Serve perché la run
# di parità prende il lease e poi lancia la gamba nativa (suo figlio) sulla stessa porta: il
# figlio non deve né aspettare la fine del padre (deadlock: il padre aspetta lui) né avviare un
# secondo broker. Il broker è lo stesso, ma le gambe della run sono sequenziali: nessun intreccio.
lease_ancestor_owns() {
  local owner p
  owner="$(lease_owner)"
  [ -n "$owner" ] || return 1
  p="$PPID"
  while [ -n "$p" ] && [ "$p" -gt 1 ] 2>/dev/null; do
    [ "$p" = "$owner" ] && return 0
    p="$(ps -o ppid= -p "$p" 2>/dev/null | tr -d ' ')"
  done
  return 1
}

# lease_acquire: 0 = preso, 1 = timeout. Un lease il cui pid non è più vivo è ORFANO (run morta
# senza `release`) e si recupera subito: altrimenti un crash bloccherebbe la porta per sempre.
lease_acquire() {
  local t=0 pid
  while ! mkdir "$LEASEDIR" 2>/dev/null; do
    pid="$(lease_owner)"
    # pid assente = il nuovo proprietario lo sta scrivendo: non si ruba, si aspetta.
    if [ -n "$pid" ] && ! kill -0 "$pid" 2>/dev/null; then
      rm -rf "$LEASEDIR"
      continue
    fi
    [ "$t" -ge "$LEASE_TICKS" ] && return 1
    sleep 0.1; t=$((t + 1))
  done
  printf '%s' "$OWNER" >"$LEASEDIR/pid"
}

# lease_drop: rilascia SOLO il lease posseduto da questa run (non quello di un altro).
lease_drop() { [ "$(lease_owner)" = "$OWNER" ] && rm -rf "$LEASEDIR"; return 0; }

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
    LEASE_OWN=0
    # Prima l'eredità, POI l'attesa: se il lease è della run che ci ha lanciati, attendere
    # significherebbe aspettare il padre che a sua volta aspetta noi.
    if lease_ancestor_owns; then
      LEASE_OWN=1            # preso dalla run che ci ha lanciati: è il NOSTRO broker
    elif lease_acquire; then
      LEASE_OWN=1
    else
      echo "broker :$PORT occupato dal lease di pid $(lease_owner) dopo $((LEASE_TICKS / 10))s: un'altra run lo sta usando" >&2
      [ "${1:-}" = "ensure" ] && echo busy
      exit 3
    fi
    if broker_up "$PORT"; then
      [ "${1:-}" = "ensure" ] && echo already || echo "broker già attivo su :$PORT"
      exit 0
    fi
    if [ "$LEASE_OWN" = 0 ]; then
      echo "broker :$PORT della run che ci ha lanciati non raggiungibile" >&2
      [ "${1:-}" = "ensure" ] && echo failed
      exit 1
    fi
    [ -f "$SRC" ] || { echo "sorgente non trovato: $SRC" >&2; lease_drop; [ "${1:-}" = "ensure" ] && echo failed; exit 1; }
    cc -O2 -o "$BIN" "$SRC" || { echo "compilazione del broker fallita" >&2; lease_drop; [ "${1:-}" = "ensure" ] && echo failed; exit 1; }
    # nohup: il broker deve sopravvivere alla shell che lo ha avviato (altrimenti muore col
    # terminale e il gate successivo trova "timeout").
    nohup "$BIN" "$PORT" "$RETRY" "$ATTEMPTS" >"${TMPDIR:-/tmp}/msg_broker_$PORT.log" 2>&1 &
    # PID registrato: `release`/`stop` fermano QUESTO processo, non "chiunque si chiami così"
    # (un pkill -f colpisce anche la shell di un'altra sessione che nomina la stessa porta).
    echo $! >"$PIDFILE"
    : >"$LEASEDIR/started"       # questo lease ha AVVIATO il broker: `release` lo fermerà
    for _ in 1 2 3 4 5 6 7 8 9 10; do
      sleep 0.2
      if broker_up "$PORT"; then
        if [ "${1:-}" = "ensure" ]; then echo started
        else echo "broker attivo su :$PORT (retry=${RETRY}ms max_attempts=$ATTEMPTS)"; fi
        exit 0
      fi
    done
    echo "broker non raggiungibile su :$PORT (vedi ${TMPDIR:-/tmp}/msg_broker_$PORT.log)" >&2
    lease_drop
    [ "${1:-}" = "ensure" ] && echo failed
    exit 1
    ;;
  stop|release)
    pid=""
    [ -f "$PIDFILE" ] && pid="$(cat "$PIDFILE" 2>/dev/null)"
    MANUAL=0; [ "${1:-}" = "stop" ] && MANUAL=1
    LEASE_OWN=0
    if [ "$(lease_owner)" = "$OWNER" ] || [ -z "$(lease_owner)" ]; then LEASE_OWN=1; fi
    # `release` da una gamba figlia: il lease è del padre, che lo rilascerà a fine run. Non si
    # ferma il broker sotto i piedi della run stessa.
    if [ "$MANUAL" = 0 ] && [ "$LEASE_OWN" = 0 ]; then
      if lease_ancestor_owns; then
        echo "broker :$PORT della run che ci ha lanciati: lo lascio a lei"
      else
        echo "broker :$PORT in uso da un'altra run (lease pid $(lease_owner)): non lo tocco"
      fi
      exit 0
    fi
    # `stop` è MANUALE: ferma il broker risulti avviato su questa porta.
    # `release` è la fine della run: ferma SOLO il broker che la run ha avviato (marker
    # `started` nel lease), altrimenti lo lascia a chi l'ha avviato.
    if [ "$MANUAL" = 1 ] || [ -f "$LEASEDIR/started" ]; then
      if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
        kill "$pid" 2>/dev/null
        sleep 0.2
        kill -0 "$pid" 2>/dev/null && kill -9 "$pid" 2>/dev/null
        rm -f "$PIDFILE"
        echo "broker fermato (:$PORT, pid $pid)"
      else
        echo "nessun broker avviato da questa run su :$PORT"
      fi
    else
      echo "nessun broker avviato da questa run su :$PORT"
    fi
    rm -f "$PIDFILE"
    lease_drop
    [ "$MANUAL" = 1 ] && rm -rf "$LEASEDIR"
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
