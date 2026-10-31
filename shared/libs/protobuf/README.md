# protobuf — codec wire-format Protocol Buffers (shim FFI self-contained)

Pacchetto gmbsuite (Prefisso C: `gmbprotobuf`). Codec **wire-format** Protocol Buffers esposto a
GMBasic via FFI (namespace **PROTOBUF**).

## Design (perché è self-contained)
Lo shim `libgmbprotobuf.dylib` implementa la lettura/scrittura del **wire-format protobuf** in **C
puro** (varint/tag/wire-types): **nessuna libreria esterna**, **nessun compilatore `.proto`** —
dipende solo da `libSystem`. Build dal sorgente (`build.sh`, eseguito dal PM).

Copre i wire type **0** (varint: int/bool/enum), **1** (64-bit), **2** (length-delimited:
string/bytes), **5** (32-bit). I byte serializzati sono esposti come **stringa esadecimale**
(evita il troncamento a NUL delle stringhe GMBasic).

## API (BASIC, namespace `PROTOBUF`)
| Membro | Firma | Semantica |
|---|---|---|
| `PROTOBUF.AVAILABLE()` | `() AS LONG` | 1 = shim disponibile |
| `PROTOBUF.NEWMSG()` | `() AS LONG` | nuovo builder |
| `PROTOBUF.ADDINT(h, field, value)` | `(LONG, LONG, LONG) AS LONG` | aggiunge campo varint |
| `PROTOBUF.ADDSTRING(h, field, s)` | `(LONG, LONG, STRING) AS LONG` | aggiunge campo string |
| `PROTOBUF.SERIALIZE(h)` | `(LONG) AS STRING` | byte → **hex** |
| `PROTOBUF.PARSE(hex)` | `(STRING) AS LONG` | hex → reader |
| `PROTOBUF.HASFIELD(h, field)` | `(LONG, LONG) AS LONG` | 1/0 |
| `PROTOBUF.GETINT(h, field)` | `(LONG, LONG) AS LONG` | valore intero |
| `PROTOBUF.GETSTRING(h, field)` | `(LONG, LONG) AS STRING` | valore stringa |
| `PROTOBUF.FREEMSG(h)` | `(LONG) AS LONG` | libera builder/reader |

## Test
`protobuf_demo.bas` (round-trip, nessun server). MiniTest `minitests/packages/require_protobuf.bas`
(deterministico: esempio canonico `field1=150, field2="testing"`).

## Riferimenti
- `protobuf.inc`, `include/gmbprotobuf.h`, `src/gmbprotobuf.c`, `build.sh`.
