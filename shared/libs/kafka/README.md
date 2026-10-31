# kafka — Kafka client (self-contained FFI shim)

gmbsuite package (C prefix: `gmbkafka`). **Kafka** client (wire protocol) exposed to GMBasic via FFI
(namespace **KAFKA**).

## Design (why it is self-contained)
The `libgmbkafka.dylib` shim implements a real subset of the **Kafka protocol** in **pure C**
(POSIX sockets): **no external library** (neither librdkafka nor anything else) — it depends only on
`libSystem`.

Implemented APIs (modern brokers 0.11+):
- **Metadata** (3, v1) — broker discovery + partition leader (with topic auto-creation).
- **Produce** (0, v3) — **RecordBatch v2** with **CRC32C**, acks=1.
- **Fetch** (1, v4) — recordset reading.

## API (BASIC, `KAFKA` namespace)
| Member | Signature | Semantics |
|---|---|---|
| `KAFKA.AVAILABLE()` | `() AS LONG` | 1 = shim available |
| `KAFKA.CONNECT(host, port, client_id)` | `(STRING, LONG, STRING) AS LONG` | handle >0, `-1` error |
| `KAFKA.PRODUCE(h, topic, partition, value)` | `(LONG, STRING, LONG, STRING) AS LONG` | error_code (0 ok) |
| `KAFKA.CONSUME(h, topic, partition, offset, timeout_ms)` | `(LONG, STRING, LONG, LONG, LONG) AS LONG` | 1 record, 0 none, -1 error |
| `KAFKA.LASTVALUE(h)` | `(LONG) AS STRING` | last value |
| `KAFKA.NEXTOFFSET(h)` | `(LONG) AS LONG` | next offset |
| `KAFKA.DISCONNECT(h)` | `(LONG) AS LONG` | 0 ok |

## Test
Ships with a broker: `docker run -d --name kafka -p 9092:9092 apache/kafka:latest`. Then
`samples/kafka_demo.bas`. MiniTest `minitests/packages/require_kafka.bas` (deterministic: AVAILABLE).
Verified end-to-end with **Apache Kafka** (KRaft single-node) in Docker: connect/metadata/produce/
consume → correct value.

## References
- `kafka.inc`, `include/gmbkafka.h`, `src/gmbkafka.c`, `build.sh`. Sample: `samples/kafka_demo.bas`.
