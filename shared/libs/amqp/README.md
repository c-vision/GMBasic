# amqp — AMQP 0-9-1 client (RabbitMQ) (self-contained FFI shim)

gmbsuite package (C prefix: `gmbamqp`). **AMQP 0-9-1** client (RabbitMQ) exposed to GMBasic via FFI
(namespace **AMQP**).

## Design (why it is self-contained)
The `libgmbamqp.dylib` shim implements **AMQP 0-9-1** in **pure C** (POSIX sockets, SASL **PLAIN**
authentication): **no external library** (neither librabbitmq nor anything else) — it depends only
on `libSystem`. Built from source (`build.sh`, run by the package manager).

Implemented path: handshake (**Connection.Start/Start-Ok, Tune/Tune-Ok, Open/Open-Ok**),
**Channel.Open**, **Queue.Declare**, **Basic.Publish** (+ content header + body), **Basic.Get**
(+ content), **Disconnect**.

## API (BASIC, `AMQP` namespace)
| Member | Signature | Semantics |
|---|---|---|
| `AMQP.AVAILABLE()` | `() AS LONG` | 1 = shim available |
| `AMQP.CONNECT(host, port, user, pass)` | `(STRING, LONG, STRING, STRING) AS LONG` | handle >0, `-1` error |
| `AMQP.DECLAREQUEUE(h, queue)` | `(LONG, STRING) AS LONG` | 0 ok |
| `AMQP.PUBLISH(h, exchange, routing_key, body)` | `(LONG, STRING, STRING, STRING) AS LONG` | 0 ok |
| `AMQP.GET(h, queue, timeout_ms)` | `(LONG, STRING, LONG) AS LONG` | 1 message, 0 empty, -1 error |
| `AMQP.LASTBODY(h)` / `AMQP.LASTROUTINGKEY(h)` | `(LONG) AS STRING` | last message |
| `AMQP.LASTSIZE(h)` | `(LONG) AS LONG` | body size |
| `AMQP.DISCONNECT(h)` | `(LONG) AS LONG` | 0 ok |

## Test
Ships with a broker: `docker run -d --name amqp -p 5672:5672 rabbitmq:3` (on Docker Desktop you may
need `--tmpfs /var/lib/rabbitmq`). Then `samples/amqp_demo.bas`.
MiniTest `minitests/packages/require_amqp.bas` (deterministic: AVAILABLE).
Verified end-to-end with **RabbitMQ** and **LavinMQ** (both AMQP 0-9-1): connect/declare/publish/get
→ correct body.

## References
- `amqp.inc`, `include/gmbamqp.h`, `src/gmbamqp.c`, `build.sh`. Sample: `samples/amqp_demo.bas`.
