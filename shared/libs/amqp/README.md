# amqp — client AMQP 0-9-1 (RabbitMQ) (shim FFI self-contained)

Pacchetto gmbsuite (Prefisso C: `gmbamqp`). Client **AMQP 0-9-1** (RabbitMQ) esposto a GMBasic via
FFI (namespace **AMQP**).

## Design (perché è self-contained)
Lo shim `libgmbamqp.dylib` implementa **AMQP 0-9-1** in **C puro** (socket POSIX, autenticazione
SASL **PLAIN**): **nessuna libreria esterna** (né librabbitmq né altro) — dipende solo da
`libSystem`. Build dal sorgente (`build.sh`, eseguito dal PM).

Percorso implementato: handshake (**Connection.Start/Start-Ok, Tune/Tune-Ok, Open/Open-Ok**),
**Channel.Open**, **Queue.Declare**, **Basic.Publish** (+ content header + body), **Basic.Get**
(+ content), **Disconnect**.

## API (BASIC, namespace `AMQP`)
| Membro | Firma | Semantica |
|---|---|---|
| `AMQP.AVAILABLE()` | `() AS LONG` | 1 = shim disponibile |
| `AMQP.CONNECT(host, port, user, pass)` | `(STRING, LONG, STRING, STRING) AS LONG` | handle >0, `-1` err |
| `AMQP.DECLAREQUEUE(h, queue)` | `(LONG, STRING) AS LONG` | 0 ok |
| `AMQP.PUBLISH(h, exchange, routing_key, body)` | `(LONG, STRING, STRING, STRING) AS LONG` | 0 ok |
| `AMQP.GET(h, queue, timeout_ms)` | `(LONG, STRING, LONG) AS LONG` | 1 msg, 0 vuota, -1 err |
| `AMQP.LASTBODY(h)` / `AMQP.LASTROUTINGKEY(h)` | `(LONG) AS STRING` | ultimo messaggio |
| `AMQP.LASTSIZE(h)` | `(LONG) AS LONG` | dimensione body |
| `AMQP.DISCONNECT(h)` | `(LONG) AS LONG` | 0 ok |

## Test
Include un broker: `docker run -d --name amqp -p 5672:5672 rabbitmq:3` (su Docker Desktop può
servire `--tmpfs /var/lib/rabbitmq`). Poi `samples/amqp_demo.bas`.
MiniTest `minitests/packages/require_amqp.bas` (deterministico: AVAILABLE).
Verificato end-to-end con **RabbitMQ** e con **LavinMQ** (entrambi AMQP 0-9-1): connect/declare/
publish/get → body corretto.

## Riferimenti
- `amqp.inc`, `include/gmbamqp.h`, `src/gmbamqp.c`, `build.sh`. Sample: `samples/amqp_demo.bas`.
