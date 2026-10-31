# grpc — unary gRPC client (self-contained FFI shim)

gmbsuite package (C prefix: `gmbgrpc`). **Unary gRPC** client exposed to GMBasic via FFI
(namespace **GRPC**).

## Design (why it is self-contained)
The `libgmbgrpc.dylib` shim implements **HTTP/2 + gRPC** in **pure C** (POSIX sockets): **no
external library** (neither grpc nor nghttp2) — it depends only on `libSystem`.

It implements: connection preface + SETTINGS, **HEADERS** (minimal HPACK: literal without indexing),
**DATA** with **gRPC framing** (1-byte flag + 4-byte BE length + message), response reading
(HEADERS/DATA/trailers with `grpc-status`), SETTINGS/PING ACK, GOAWAY.

The **message payload** is **hexadecimal** (protobuf bytes contain NULs): use it together with the
**`protobuf`** package to build/read messages.

## API (BASIC, `GRPC` namespace)
| Member | Signature | Semantics |
|---|---|---|
| `GRPC.AVAILABLE()` | `() AS LONG` | 1 = shim available |
| `GRPC.INVOKE(host, port, path, req_hex)` | `(STRING, LONG, STRING, STRING) AS LONG` | grpc-status (0 = OK), `-1` transport error |
| `GRPC.LASTRESPONSE()` | `() AS STRING` | response message (hex) |
| `GRPC.LASTSTATUS()` | `() AS LONG` | last grpc-status |

## Test
Ships with a h2c gRPC server: `docker run -d --name grpc -p 9000:9000 moul/grpcbin:latest`. Then
`samples/grpc_demo.bas` (calls `GRPCBin.DummyUnary`). MiniTest `minitests/packages/require_grpc.bas`
(deterministic: AVAILABLE). Verified end-to-end with **moul/grpcbin** in Docker: message echo.

## References
- `grpc.inc`, `include/gmbgrpc.h`, `src/gmbgrpc.c`, `build.sh`. Sample: `samples/grpc_demo.bas`.
