# TODO — open work

Short index of what is **not** done yet. Details stay next to the code (comments, tests) and in
the pull requests; this file does not duplicate them.

## The two IDEs — outside the repo until complete

`GMBasicIDE/` (native macOS GUI, VB6-style) and `GMBasicTuiIDE/` (TUI written in BASIC) are
incomplete and deliberately **not** part of this repository: they are in `.gitignore` and will be
published when finished. The REPL's own `IDE` command (`REPL/ide.c`, shared logic in
`gmb_runtime_c/src/gmb_ide.c`) **is** in the repo — it belongs to the REPL, not to the two projects.

## Open work

| Area | What is missing |
|---|---|
| Test pipeline | **C first**: `tools/sh/c99_emit_gate.sh` (every `.bas` must emit on C99) and `tools/sh/test_c99_first.sh` (gate → native run → optional parity) are the entry points; the C# suite runs `C99Emission.Require` before every sample test (22 classes). Full-corpus parity is green (2300 files × 4 legs) but runs by hand, not yet in CI. |
| Single C REPL | The C REPL must drive the **C compiler** through its options and keep every function of the C world; `GMBasicSharp` must remain optional and removable. |
| C# engine | Decide whether `GMBasicSharp` is deprecated. It is the **independent oracle** of the cross-leg parity suite, so deprecating it means rewriting that suite in C. |
| Release | RIDs `osx-x64`, `linux-x64`, `linux-arm64` (the CI already builds and gates them); `win-*` wait for the POSIX port the compiled groups need. `gmbcs` is not shipped. |
| C99Native performance | Measured optimizations: bounds induction, pipeline, data-driven inlining with an arbiter for LLM-proposed changes. |
| Interpreters | PCodeVM quickening ("cliff") and VMC quickening. |
| Backend benchmark | A four-backend benchmark with a regression gate. |
| Graphics | The GFX domain work is open. |
| Libraries | GMBSuite: migrate the remaining builtins to FFI libraries, or decide where the boundary is. |

### scipy → FFI: funzioni da pacchetto, NON builtin (deciso/applicato 2026-11-23)

Principio: le builtin **base** del dialetto restano; le funzioni `scipy.*` (fft/interpolate/signal)
sono tipiche da **FFI** e non appartengono alla dotazione standard.

**Fatto.** `RFFT`/`IRFFT` nel pacchetto `signal` (`ofx_signal_rfft/irfft` + `signal.inc`);
`SPLREP`/`SPLEV` nel pacchetto `math` (namespace `INTERP`, `interp.inc`) con `gmi_splrep` portato a
**Thomas O(n)**. I MiniTest usano `IMPORT` e sono verdi sulle 4 gambe. I 4 builtin sono **usciti
dalla superficie canonica** (982 → 978, `registry-export --all`); un programma con
`NUMERICAL.Rfft` non compila più.

**Resta (pulizia, non superficie):**

- [ ] rimuovere le implementazioni ora irraggiungibili: case in `IlProgramCompiler.Builtins.cs`,
      dictionary PCode (`FFT_RFFT`/`FFT_IRFFT`, `INTERP_*`), 10 punti in `gmb_c99_emit*.c`,
      case VM/C (`gmb_builtin.c`), e i metodi `IlRuntime`/`SciPyP1` (`Rfft`, `Irfft`, `Splrep`,
      `Splev`, `CubicSplineRep`, `SolveNotAKnot*`) → archivio in Deprecated;
- [ ] `SIGNAL.WELCH` (builtin che ritorna un handle di matrice) e `SAVGOL`: prima esporre
      `WELCH` in `signal.inc` come array, poi spegnere il builtin;
- [ ] il gate `check_enum_ids` ha un DRIFT **pre-esistente** (`gmb_vm_c/gmb_syscall_codes.h` vs
      `SysCalls.cs`, dal commit `aea3d3f5` clock): rigenerare con `tools/check_enum_ids --gen`.

| Local AI | CPU-only MoE streaming engine (no GPU, no MLX). |
| Namespaces | Math / Stats / TimeSeries taxonomy. |
| Toolchain | Zig cross-compilation and RIDs that need no system toolchain. |
| Weight space | Editable weights through LLVM. |

## Environment-only test gaps

`llm/` (4 MiniTests), `train/` and `tray/` need a real model, the training registry and a
main-thread pump. They are skipped by the cross-leg runner: they are not failing code.
