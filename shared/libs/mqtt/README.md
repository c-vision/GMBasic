# mqtt — MQTT 3.1.1 client (self-contained FFI shim)

gmbsuite package (C prefix: `gmbmqtt`). **MQTT 3.1.1** client (QoS 0) exposed to GMBasic via FFI
(namespace **MQTT**).

## Design (why it is self-contained)
The `libgmbmqtt.dylib` shim implements the MQTT 3.1.1 protocol **in pure C** (POSIX sockets):
**no external library** (neither mosquitto nor anything else) — it depends only on `libSystem`. The
build (`build.sh`, run by the package manager) compiles the source.

## API (BASIC, `MQTT` namespace)
| Member | Signature | Semantics |
|---|---|---|
| `MQTT.AVAILABLE()` | `() AS LONG` | 1 = shim available |
| `MQTT.CONNECT(host, port, cid)` | `(STRING, LONG, STRING) AS LONG` | handle >0, `-1` error |
| `MQTT.PUBLISH(h, topic, payload)` | `(LONG, STRING, STRING) AS LONG` | 0 ok (QoS0) |
| `MQTT.SUBSCRIBE(h, topic)` | `(LONG, STRING) AS LONG` | 0 ok (QoS0) |
| `MQTT.RECEIVE(h, timeout_ms)` | `(LONG, LONG) AS LONG` | 1 message, 0 timeout, -1 error |
| `MQTT.LASTTOPIC(h)` / `MQTT.LASTPAYLOAD(h)` | `(LONG) AS STRING` | last message |
| `MQTT.DISCONNECT(h)` | `(LONG) AS LONG` | 0 ok |

## Test
Ships with a broker: `docker run -d --name mq -p 1883:1883 eclipse-mosquitto:2 sh -c "printf 'listener 1883\nallow_anonymous true\n' > /mosquitto/config/mosquitto.conf && mosquitto -c /mosquitto/config/mosquitto.conf"`.
Then `samples/mqtt_demo.bas`. MiniTest `minitests/packages/require_mqtt.bas` (deterministic:
AVAILABLE + connect to a closed port).

## References
- `mqtt.inc`, `include/gmbmqtt.h`, `src/gmbmqtt.c`, `build.sh`. Sample: `samples/mqtt_demo.bas`.
