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
| String runtime | **Risolto (2026-11-24)** il bug di aliasing sugli array dinamici di stringhe: era nell'emitter C99, non nel runtime. `emit_assignment` liberava il valore precedente del target anche quando il RHS era una sorgente in prestito (variabile, elemento di array, campo), lasciando un alias a memoria liberata (`ln = arr(k)` in un ciclo → elementi vuoti; `b = a` + `a = ...` → b appeso). Ora il RHS in prestito si **copia** (`gmb_string_from_bytes`), i temporanei restano sul percorso veloce: MiniTest `strings/string_assign_copy.bas` su 4 gambe. Le stringhe C99 restano non-refcountate: la copia è il contratto. |
| `StringsExamples.bas` (C99) | **Risolto (2026-11-24)**, `parity4` PASS. Tre cause: (a) `L_len` mai dichiarato con `LET x() = SPLIT(...)` su array a bound fisso; (b) SPLIT/ATOKENS classificati `gmb_list_t`; (c) `lookup_var` non normalizzava il suffisso BASIC, quindi `LIST` (il lexer toglie `$` quando il nome coincide con un TokenKind) non trovava la variabile `LIST$` e i rami `UBOUND`/`JOIN`/`PRINT AI.LIST` scattavano sul builtin omonimo (`gmb_provider_list_names`/`gmb_list_count`/`gmb_list_join` su un `gmb_string_t*`). Commit `09848bcd`, `226390c8`, `12a047f5`. |
| Strings (altri) | **`StringsInterpolation` RISOLTO** (2026-11-24): i format specifier `{expr:fmt}` erano ignorati — il parser C li buttava, `FormatUtils.FormatNumeric` (C#) trattava gli specificatori standard come LETTERALI (`{255:X}` → "X") e il bytecode (PCodeVM/VM-C) non emetteva FORMAT. Ora il FORMAT è applicato su tutte e 4 le gambe (`gmb_format` per C99, `OP_CALL_BUILTIN FORMAT` per il bytecode, `ToString(spec, InvariantCulture)` per X/D/F). Commit `87bc35f6`. **`StringsStringBuilder`**: `Capacity` allineata a .NET (16+raddoppio) su VM/C (`a7661f86`); resta un DIFF per una variabile chiamata `pi`: PCode/VM-C risolvono la COSTANTE π invece della variabile dichiarata (`DIM pi = 3.14159` → `sb.Append(pi)` dà π). Stessa classe del caso `LIST$`/`LIST` ma nella risoluzione simboli C#. **`StringsRegex`**: `Match.Length` 4 vs 3 (motore regex, pre-esistente). |

### Gate `no_stub` — sistemato il 2026-11-24

Il gate non finiva più (>30 min, prima fase mai stampata): estraeva il corpo di ogni handler con
`BodyOf`, che cerca il nome dall'inizio del file e scorre il sorgente con `MID$` → O(n²) su
`gmb_builtin.c` (930 KB, 1047 handler) e `gmb_c99_emit.c` (2,2 MB). Ora spezza il file una volta
(`SPLIT`) ed estrae per riga/colonna del match (`BodyAtLine`): **2m52s**, `0 colpa / 0 reported`.
Self-test: `--selftest` (3 issue attese su 4 handler) e `--bodytest` (5 forme di definizione,
confronto con `BodyOf`). Restano O(n²) solo i file < 200 KB (fallback).

### scipy → FFI: funzioni da pacchetto, NON builtin (APPLICATO 2026-11-23/24)

Principio: le builtin **base** del dialetto restano; le funzioni `scipy.*` (fft/interpolate/signal)
sono tipiche da **FFI** e non appartengono alla dotazione standard.

**Fatto — superficie 982 → 977.**

| ex builtin | dove vive | commit |
|---|---|---|
| `NUMERICAL.Rfft` / `Irfft` | `SIGNAL.RFFT` / `SIGNAL.IRFFT` (pacchetto `signal`) | `47121097` + `537c6b6e` |
| `NUMERICAL.Splrep` / `Splev` | `INTERP.SPLREP` / `INTERP.SPLEV` (pacchetto `math`, Thomas O(n)) | `ed0ecef8` + `537c6b6e` |
| `SIGNAL.WELCH` | `SIGNAL.WELCH` array interleaved + `WELCHFREQS`/`WELCHPSD` | `72c14dd4` |

I MiniTest/sample usano `IMPORT` e sono verdi sulle 4 gambe. Le implementazioni managed e i
percorsi emitter/VM-C irraggiungibili sono stati **rimossi** (`0f7074a0`, `68e75680`) e archiviati
in `GMBasicMonoRepo-Deprecated/archive-2026-11-23-scipy-ffi-step3/` e
`.../archive-2026-11-24-welch-ffi/`.

**Resta (non superficie):**

- [x] ~~`gmb_rfft_c99`/`gmb_irfft_c99`/`gmb_splrep_c99`/`gmb_splev_c99`/`gmb_welch_c99` nel
      runtime C~~ → **rimossi** con gli altri bridge C99 di signal/fft/interpolate
      (`gmb_savgol_c99`, `gmb_resample_c99`, `gmb_decimate_c99`, `gmb_firwin_c99`,
      `gmb_find_peaks_alloc`): erano senza chiamanti. Restano i kernel senza suffisso (`*_flat`)
      e `gmb_matrix_from_flat` (usato dall'emitter). Commit `2fa54283`.
- [x] ~~gate `check_enum_ids` con DRIFT~~ → **verde**: il drift era il WIP dell'altra sessione su
      `SysCalls.cs` + `gmb_syscall_codes.h`, rigenerato da loro; HEAD era già consistente.

| Local AI | CPU-only MoE streaming engine (no GPU, no MLX). |
| Namespaces | Math / Stats / TimeSeries taxonomy. |
| Toolchain | Zig cross-compilation and RIDs that need no system toolchain. |
| Weight space | Editable weights through LLVM. |

## Environment-only test gaps

`llm/` (4 MiniTests), `train/` and `tray/` need a real model, the training registry and a
main-thread pump. They are skipped by the cross-leg runner: they are not failing code.
