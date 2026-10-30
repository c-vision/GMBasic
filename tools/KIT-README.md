# Strumenti C# del monorepo (GMBasicSharp)

Harness CLI e runner di parità per il progetto **C#** (`IL`, `P-Code`, `VM/C` via host nativo).
Tutti gli harness vivono in `GMBasicSharp/` e linkano i progetti in `common/` e `Executor/`.

## Harness

| strumento | uso |
|---|---|
| `test_ilrun` | `dotnet tools/runners/test_ilrun/bin/Debug/net10.0/test_ilrun.dll --engine=il <file.bas>` (anche `--engine=pcode`/`vmc`) |
| `test_pcode` | `dotnet tools/runners/test_pcode/bin/Debug/net10.0/test_pcode.dll <file.bas>` |
| `test_vmrun` | `dotnet tools/runners/test_vmrun/bin/Debug/net10.0/test_vmrun.dll <file.bas> [timeout_s]` — VM/C con anti-stale di `gmb_vm_host` e cleanup browser/CDP |
| `test_astdump` | dump AST di un `.bas` |

Input scriptato (test interattivi `@GET`/`READ`): `GMB_TUI_SCRIPT=$'a\rb\r'`.
Wrapper: `tools/sh/harness.sh <il|pcode|vmc|ast> <file.bas>` (imposta
`GMBASIC_ROOT=../GMBasic` e `DYLD_LIBRARY_PATH` per le lib native).

## Parità MiniTest (4 gambe: IL, PCode, VM-C, C99Native)

```bash
tools/sh/test_minitests_parity.sh controls/       # filtro mirato
tools/sh/test_minitests_parity.sh --all          # suite completa (esplicita)
tools/sh/test_minitests_parity.sh --interactive get_valid_parity.bas
```

Legge i MiniTest da `shared/minitests/`, usa `tests/GMBASIC.IlBackend.Tests` e i binari
C in `GMBasic/native/gmb_vm_c/build`. `GMB_MINITEST_BUILD=1` forza la build del progetto test.

## VM/C e test interattivi

`tools/sh/harness.sh vmc` usa `test_vmrun` (Conformance `vmtest` + anti-stale host). Per i test
**interattivi** (`@GET`/`READ`) il chunk generato dal compilatore C# non riceve l'input
scriptato: usare il percorso nativo (come nel repo C):
`tools/sh/gmb_c99native_run.sh` per C99, oppure `gmb_chunk_driver`+`gmb_vm_host` in
`GMBasic/native/...` (verificato: `gv=ab`). I test interattivi sono `TAG: interactive`.

## Note

- La CWD è sempre la cartella del sample (nessun artefatto in root).
- `GMBasicSharp/test_*` e `tools/` sono **strumenti**: non fanno parte della
  superficie BASIC; i test veri stanno in `shared/minitests/`.
