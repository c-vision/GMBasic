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

### scipy → FFI: funzioni da pacchetto, NON builtin (deciso 2026-11-23)

Principio: le builtin **base** del dialetto restano; le funzioni `scipy.*` (fft/interpolate/signal)
sono tipiche da **FFI** e non appartengono alla dotazione standard.

Fatto: `RFFT`/`IRFFT` nel pacchetto `signal` (`ofx_signal_rfft/irfft` + `signal.inc`) e
`SPLREP`/`SPLEV` nel pacchetto `math` (namespace `INTERP`, `interp.inc`), con `gmi_splrep` portato
a **Thomas O(n)** (prima era denso n×n). I MiniTest (`analysis/{rfft,irfft,splrep,splev,
analysis_all}.bas`) usano già `IMPORT` e sono verdi sulle 4 gambe.

Resta il **passo 3 — rimozione dalla superficie** (builtin ancora attivi):

- [ ] `shared/spec/registry/surface.csv` (righe `NUMERICAL.{Irfft,Rfft,Splev,Splrep}`): file
      **generato** da `tools/registry-export` → rigenerare, non editare a mano;
- [ ] `BackendSupport/Registry/{IL,PCodeVM,VMC,C99Native}.cs`: 4 voci ciascuno;
- [ ] `IlProgramCompiler.Builtins.cs`: 4 case (`InterpSplrep/Splev`, `FftRfft/Irfft`);
- [ ] PCode `PCodeProcessor.Builtins.Fft.cs`: `FFT_RFFT`/`FFT_IRFFT` nella dictionary (+ mapping
      TokenKind→BuiltInFunction); `SPLREP/SPLEV` dove sono mappati;
- [ ] C99 `gmb_c99_emit.c` + `gmb_c99_emit_builtin.c`: 10 punti;
- [ ] VM/C `gmb_builtin.c`: i case di `gmb_splrep/gmb_rfft/gmb_irfft` (+ `splev`);
- [ ] `IlRuntime` e `CommonLib.Numerics` (`SciPyP1`: `Rfft`, `Irfft`, `Splrep`, `Splev`,
      `CubicSplineRep`, `SolveNotAKnotDense/Tridiagonal`) → archivio in Deprecated;
- [ ] rigenerare manifest/audit (`tools/registry-export --all`, `tools/audit -- --all`):
      baseline superficie 982 → 978.

Stesso trattamento (ma serve prima il wrapper nel `.inc`): `SIGNAL.WELCH` (builtin che ritorna un
handle di matrice; `signal.inc` oggi non lo espone) e `SAVGOL` (wrapper già presente).

| Local AI | CPU-only MoE streaming engine (no GPU, no MLX). |
| Namespaces | Math / Stats / TimeSeries taxonomy. |
| Toolchain | Zig cross-compilation and RIDs that need no system toolchain. |
| Weight space | Editable weights through LLVM. |

## Environment-only test gaps

`llm/` (4 MiniTests), `train/` and `tray/` need a real model, the training registry and a
main-thread pump. They are skipped by the cross-leg runner: they are not failing code.
