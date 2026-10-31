# nats — NATS client (self-contained FFI shim)

gmbsuite package (C prefix: `gmbnats`). **NATS core** client exposed to GMBasic via FFI (namespace
**NATS**).

## Design (why it is self-contained)
The `libgmbnats.dylib` shim implements the **NATS text protocol** in **pure C** (POSIX sockets):
**no external library** — it depends only on `libSystem`. Built from source (`build.sh`, run by the
package manager).

## API (BASIC, `NATS` namespace)
| Member | Signature | Semantics |
|---|---|---|
| `NATS.AVAILABLE()` | `() AS LONG` | 1 = shim available |
| `NATS.CONNECT(host, port, name)` | `(STRING, LONG, STRING) AS LONG` | handle >0, `-1` error |
| `NATS.SUBSCRIBE(h, subject)` | `(LONG, STRING) AS LONG` | 0 ok |
| `NATS.PUBLISH(h, subject, payload)` | `(LONG, STRING, STRING) AS LONG` | 0 ok |
| `NATS.RECEIVE(h, timeout_ms)` | `(LONG, LONG) AS LONG` | 1 message, 0 timeout, -1 error |
| `NATS.LASTSUBJECT(h)` / `NATS.LASTPAYLOAD(h)` | `(LONG) AS STRING` | last message |
| `NATS.DISCONNECT(h)` | `(LONG) AS LONG` | 0 ok |

## Test
Ships with a server: `docker run -d --name nats -p 4222:4222 nats:latest`. Then `samples/nats_demo.bas`.
MiniTest `minitests/packages/require_nats.bas` (deterministic: AVAILABLE).

## References
- `nats.inc`, `include/gmbnats.h`, `src/gmbnats.c`, `build.sh`. Sample: `samples/nats_demo.bas`.
