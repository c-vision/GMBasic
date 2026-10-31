# protobuf — Protocol Buffers wire-format codec (self-contained FFI shim)

gmbsuite package (C prefix: `gmbprotobuf`). Protocol Buffers **wire-format** codec exposed to
GMBasic via FFI (namespace **PROTOBUF**).

## Design (why it is self-contained)
The `libgmbprotobuf.dylib` shim implements reading/writing of the **protobuf wire format** in
**pure C** (varint/tag/wire types): **no external library**, **no `.proto` compiler** — it depends
only on `libSystem`. Built from source (`build.sh`, run by the package manager).

It covers wire types **0** (varint: int/bool/enum), **1** (64-bit), **2** (length-delimited:
string/bytes), **5** (32-bit). Serialized bytes are exposed as a **hex string** (this avoids the
NUL truncation of GMBasic strings).

## API (BASIC, `PROTOBUF` namespace)
| Member | Signature | Semantics |
|---|---|---|
| `PROTOBUF.AVAILABLE()` | `() AS LONG` | 1 = shim available |
| `PROTOBUF.NEWMSG()` | `() AS LONG` | new builder |
| `PROTOBUF.ADDINT(h, field, value)` | `(LONG, LONG, LONG) AS LONG` | append a varint field |
| `PROTOBUF.ADDSTRING(h, field, s)` | `(LONG, LONG, STRING) AS LONG` | append a string field |
| `PROTOBUF.SERIALIZE(h)` | `(LONG) AS STRING` | bytes → **hex** |
| `PROTOBUF.PARSE(hex)` | `(STRING) AS LONG` | hex → reader |
| `PROTOBUF.HASFIELD(h, field)` | `(LONG, LONG) AS LONG` | 1/0 |
| `PROTOBUF.GETINT(h, field)` | `(LONG, LONG) AS LONG` | integer value |
| `PROTOBUF.GETSTRING(h, field)` | `(LONG, LONG) AS STRING` | string value |
| `PROTOBUF.FREEMSG(h)` | `(LONG) AS LONG` | free builder/reader |

## Tests
`protobuf_demo.bas` (round-trip, no server). MiniTest `minitests/packages/require_protobuf.bas`
(deterministic: canonical example `field1=150, field2="testing"`).

## References
- `protobuf.inc`, `include/gmbprotobuf.h`, `src/gmbprotobuf.c`, `build.sh`.
