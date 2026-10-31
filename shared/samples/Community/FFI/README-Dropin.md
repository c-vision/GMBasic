# FfiDropinLibs — FFI drop-in (`include/` + `libs/`)

Esempio self-contained dell'UX **drop-in** FFI: le dichiarazioni in un `.inc` accanto al
programma + la libreria nativa in `libs/`, **senza pacchetto REQUIRE** e senza manifest.

| File | Ruolo |
|---|---|
| `FfiDropinLibs.bas` | Programma BASIC (`IMPORT FFIDROPIN_DEMO` + chiamate) |
| `FfiDropin.inc` | Dichiarazioni `DECLARE ... LIB "libvec_module"` (namespace `FFIDROPIN_DEMO`) |
| `libs/libvec_module.dylib` | Libreria nativa (risolta dall'auto-discovery `libs/`) |
| `Makefile` | Ricompila `libs/` da `NativeVec/vec_module.c` |

## Build & run

```bash
cd samples/Community/FFI
make            # -> libs/libvec_module.dylib (macOS) / .so (Linux)
# esegui FfiDropinLibs.bas con un runner (test_ilrun / test_vmrun / Executor)
```

## Ordine di ricerca delle librerie (`Guide/FFI-Guida-Utente.md` §9)

1. nome as-is (CWD) · 2. nome+estensione · 3. **`libs/` walk-up** · 4. preload REQUIRE.

Kill-switch: `GMB_FFI_NO_LIBS_SCAN=1` disattiva il passo 3 su tutte le gambe.
