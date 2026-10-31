# grpc — client gRPC unario (shim FFI self-contained)

Pacchetto gmbsuite (Prefisso C: `gmbgrpc`). Client **gRPC unario** esposto a GMBasic via FFI
(namespace **GRPC**).

## Design (perché è self-contained)
Lo shim `libgmbgrpc.dylib` implementa **HTTP/2 + gRPC** in **C puro** (socket POSIX): **nessuna
libreria esterna** (né grpc né nghttp2) — dipende solo da `libSystem`.

Implementa: connection preface + SETTINGS, **HEADERS** (HPACK minimale: literal without indexing),
**DATA** con **framing gRPC** (flag 1 byte + lunghezza 4 byte BE + messaggio), lettura risposta
(HEADERS/DATA/trailers con `grpc-status`), SETTINGS/PING ACK, GOAWAY.

Il **payload del messaggio** è in **esadecimale** (i byte protobuf contengono NUL): si usa in
combinazione col pacchetto **`protobuf`** per costruire/leggere i messaggi.

## API (BASIC, namespace `GRPC`)
| Membro | Firma | Semantica |
|---|---|---|
| `GRPC.AVAILABLE()` | `() AS LONG` | 1 = shim disponibile |
| `GRPC.INVOKE(host, port, path, req_hex)` | `(STRING, LONG, STRING, STRING) AS LONG` | grpc-status (0 = OK), `-1` err trasporto |
| `GRPC.LASTRESPONSE()` | `() AS STRING` | messaggio di risposta (hex) |
| `GRPC.LASTSTATUS()` | `() AS LONG` | ultimo grpc-status |

## Test
Include un server gRPC h2c: `docker run -d --name grpc -p 9000:9000 moul/grpcbin:latest`. Poi
`samples/grpc_demo.bas` (chiama `GRPCBin.DummyUnary`). MiniTest `minitests/packages/require_grpc.bas`
(deterministico: AVAILABLE). Verificato end-to-end con **moul/grpcbin** in Docker: eco del messaggio.

## Riferimenti
- `grpc.inc`, `include/gmbgrpc.h`, `src/gmbgrpc.c`, `build.sh`. Sample: `samples/grpc_demo.bas`.
