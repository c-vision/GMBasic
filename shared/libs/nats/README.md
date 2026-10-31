# nats — client NATS (shim FFI self-contained)

Pacchetto gmbsuite (Prefisso C: `gmbnats`). Client **NATS core** esposto a GMBasic via FFI (namespace **NATS**).

## Design (perché è self-contained)
Lo shim `libgmbnats.dylib` implementa il **protocollo testuale NATS** in **C puro** (socket POSIX):
**nessuna libreria esterna** — dipende solo da `libSystem`. Build dal sorgente (`build.sh`, eseguito dal PM).

## API (BASIC, namespace `NATS`)
| Membro | Firma | Semantica |
|---|---|---|
| `NATS.AVAILABLE()` | `() AS LONG` | 1 = shim disponibile |
| `NATS.CONNECT(host, port, name)` | `(STRING, LONG, STRING) AS LONG` | handle >0, `-1` err |
| `NATS.SUBSCRIBE(h, subject)` | `(LONG, STRING) AS LONG` | 0 ok |
| `NATS.PUBLISH(h, subject, payload)` | `(LONG, STRING, STRING) AS LONG` | 0 ok |
| `NATS.RECEIVE(h, timeout_ms)` | `(LONG, LONG) AS LONG` | 1 msg, 0 timeout, -1 err |
| `NATS.LASTSUBJECT(h)` / `NATS.LASTPAYLOAD(h)` | `(LONG) AS STRING` | ultimo messaggio |
| `NATS.DISCONNECT(h)` | `(LONG) AS LONG` | 0 ok |

## Test
Include un server: `docker run -d --name nats -p 4222:4222 nats:latest`. Poi `samples/nats_demo.bas`.
MiniTest `minitests/packages/require_nats.bas` (deterministico: AVAILABLE).

## Riferimenti
- `nats.inc`, `include/gmbnats.h`, `src/gmbnats.c`, `build.sh`. Sample: `samples/nats_demo.bas`.
