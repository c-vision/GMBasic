# NativeGo — FFI Go (cgo) puro per GMBasic (c-abi, senza `gmb_ffi.h`)

Modulo Go **self-contained** chiamabile da BASIC via `DECLARE`. Nessun header
GMBasic richiesto: solo `//export` + `-buildmode=c-shared`.

## File

| File | Ruolo |
|---|---|
| `math_go.go` | Modulo Go: `math_add`, `math_scale`, `math_isprime`, `math_factorial`, `math_label`, `math_label_free` |
| `MathGo.bas` | Programma BASIC che usa `DECLARE FUNCTION ... LIB "./NativeGo/math_go"` |
| `build.sh` | `go build -buildmode=c-shared` → `libmath_go.dylib`/`.so` |

## Build & run

```bash
cd samples/Community/FFI/NativeGo
sh build.sh        # → libmath_go.dylib (+ libmath_go.h generato da cgo)
# poi esegui MathGo.bas
```

## Regole c-abi (limiti Go)

- **Scalari**: `int64`/`float64` → `LONG`/`DOUBLE` BASIC, mappati direttamente.
- **Stringhe OUT**: `C.CString` alloca in **C heap**; GMBasic copia subito, ma il
  buffer C resta allocato → esporre sempre `*_free` (`math_label_free` via `C.free`).
- **Stringhe IN**: `C.GoString` copia — mai tenere `*C.char` dopo il call.
- **LIMITE**: Go NON può ricevere struct C **by-value** nei `//export` (solo
  puntatori). Per i tipi avanzati di `gmb_ffi.h` (`gmb_val_t`/`gmb_list_t`) Go
  non è adatto: usare C/C++ per `gmb-native`, Go solo per `c-abi` scalari/stringhe.
- **Panic**: un `panic` in un `//export` NON è contenibile (Go runtime aborta) —
  gestire gli errori internamente e ritornare sentinelle.
