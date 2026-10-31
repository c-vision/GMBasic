# mqtt — client MQTT 3.1.1 (shim FFI self-contained)

Pacchetto gmbsuite (Prefisso C: `gmbmqtt`). Client **MQTT 3.1.1** (QoS 0) esposto a GMBasic via FFI
(namespace **MQTT**).

## Design (perché è self-contained)
Lo shim `libgmbmqtt.dylib` implementa il protocollo MQTT 3.1.1 **in C puro** (socket POSIX):
**nessuna libreria esterna** (né mosquitto né altro) — dipende solo da `libSystem`. Il build
(`build.sh`, eseguito dal PM) compila il sorgente.

## API (BASIC, namespace `MQTT`)
| Membro | Firma | Semantica |
|---|---|---|
| `MQTT.AVAILABLE()` | `() AS LONG` | 1 = shim disponibile |
| `MQTT.CONNECT(host, port, cid)` | `(STRING, LONG, STRING) AS LONG` | handle >0, `-1` err |
| `MQTT.PUBLISH(h, topic, payload)` | `(LONG, STRING, STRING) AS LONG` | 0 ok (QoS0) |
| `MQTT.SUBSCRIBE(h, topic)` | `(LONG, STRING) AS LONG` | 0 ok (QoS0) |
| `MQTT.RECEIVE(h, timeout_ms)` | `(LONG, LONG) AS LONG` | 1 msg, 0 timeout, -1 err |
| `MQTT.LASTTOPIC(h)` / `MQTT.LASTPAYLOAD(h)` | `(LONG) AS STRING` | ultimo messaggio |
| `MQTT.DISCONNECT(h)` | `(LONG) AS LONG` | 0 ok |

## Test
Include un broker: `docker run -d --name mq -p 1883:1883 eclipse-mosquitto:2 sh -c "printf 'listener 1883\nallow_anonymous true\n' > /mosquitto/config/mosquitto.conf && mosquitto -c /mosquitto/config/mosquitto.conf"`.
Poi `samples/mqtt_demo.bas`. MiniTest `minitests/packages/require_mqtt.bas` (deterministico: AVAILABLE + connect a porta chiusa).

## Riferimenti
- `mqtt.inc`, `include/gmbmqtt.h`, `src/gmbmqtt.c`, `build.sh`. Sample: `samples/mqtt_demo.bas`.
