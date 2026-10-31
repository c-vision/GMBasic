# kafka — client Kafka (shim FFI self-contained)

Pacchetto gmbsuite (Prefisso C: `gmbkafka`). Client **Kafka** (wire protocol) esposto a GMBasic via
FFI (namespace **KAFKA**).

## Design (perché è self-contained)
Lo shim `libgmbkafka.dylib` implementa un sottoinsieme reale del **protocollo Kafka** in **C puro**
(socket POSIX): **nessuna libreria esterna** (né librdkafka né altro) — dipende solo da `libSystem`.

API implementate (broker moderni 0.11+):
- **Metadata** (3, v1) — scoperta broker + leader di partizione (con auto-creazione topic).
- **Produce** (0, v3) — **RecordBatch v2** con **CRC32C**, acks=1.
- **Fetch** (1, v4) — lettura recordset.

## API (BASIC, namespace `KAFKA`)
| Membro | Firma | Semantica |
|---|---|---|
| `KAFKA.AVAILABLE()` | `() AS LONG` | 1 = shim disponibile |
| `KAFKA.CONNECT(host, port, client_id)` | `(STRING, LONG, STRING) AS LONG` | handle >0, `-1` err |
| `KAFKA.PRODUCE(h, topic, partition, value)` | `(LONG, STRING, LONG, STRING) AS LONG` | error_code (0 ok) |
| `KAFKA.CONSUME(h, topic, partition, offset, timeout_ms)` | `(LONG, STRING, LONG, LONG, LONG) AS LONG` | 1 record, 0 nessuno, -1 err |
| `KAFKA.LASTVALUE(h)` | `(LONG) AS STRING` | ultimo valore |
| `KAFKA.NEXTOFFSET(h)` | `(LONG) AS LONG` | prossimo offset |
| `KAFKA.DISCONNECT(h)` | `(LONG) AS LONG` | 0 ok |

## Test
Include un broker: `docker run -d --name kafka -p 9092:9092 apache/kafka:latest`. Poi
`samples/kafka_demo.bas`. MiniTest `minitests/packages/require_kafka.bas` (deterministico: AVAILABLE).
Verificato end-to-end con **Apache Kafka** (KRaft single-node) in Docker: connect/metadata/produce/
consume → valore corretto.

## Riferimenti
- `kafka.inc`, `include/gmbkafka.h`, `src/gmbkafka.c`, `build.sh`. Sample: `samples/kafka_demo.bas`.
