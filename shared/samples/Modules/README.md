# samples/modules — module resolution (O7)

Single example for **every** place where the compiler looks for a BASIC module
(`*.bas`/`*.inc`). It applies to the C# Orchestrator and to the native drivers: both read the same
configuration, so the sample compiles and runs in the same way.

Entry: [`FolderResolution/AllFolders.bas`](FolderResolution/AllFolders.bas).

```
samples/modules/
  FolderResolution/                 <- PROGRAM root (where the entry lives)
    AllFolders.bas                  IMPORT of one namespace per folder
    app.settings.json               importFolders + importFiles + compiler.searchFolders
    include/LocalInclude.bas        searchFolders "include"
    Controls/LocalControl.bas       searchFolders "Controls"
    deep/nested/Nested.bas          RECURSIVE scan of the root
    chain/Chain.bas                 IMPORT DEMO_DEEP (transitive graph)
    libs/widgets/Widget.bas         searchFolders "{project}/libs/*" (glob)
    libs/helpers/Deep.bas           loaded only because Chain.bas imports it
  shared/SharedModule.bas           importFolders "../shared" (OUTSIDE the root)
  pinned/PinnedOnly.bas             importFiles "../pinned/PinnedOnly.bas" (OUTSIDE the root)
```

## Rules (the same in the two paths)

1. **Program root, recursive**: every `.bas`/`.inc` under the entry folder is a
   candidate, at any depth.
2. **`importFolders`** (system + app settings): relative to the project **and** to the
   install root. They also reach folders outside the program tree.
3. **`importFiles`**: files always loaded, relative to the project (then to the install).
4. **`compiler.searchFolders`**: the historical list `include` · `Controls` · `controls` ·
   `{install}/include` · `{install}/Controls` · `{install}/libs/*` is only the **default**; a
   project replaces it from `app.settings.json`. Placeholders `{project}`/`{install}`, trailing
   `*` = every subfolder, absolute paths allowed. The scan of every entry is
   recursive. A list present **replaces** the global one; `[]` disables them all.
5. **Shadowing**: a file of the project/of the imports wins, by NAME, over the one of a global
   folder.
6. **`IMPORT`-driven**: being a candidate is not enough; only what is
   reachable from the entry's `IMPORT`s enters the build, followed also inside the loaded modules
   (`DEMO_CHAIN` → `DEMO_DEEP`).

## Execution

```bash
# project CLI (O7): discovery + check + build + run
native/gmb_compiler_c/build/gmb check --project samples/modules/FolderResolution
native/gmb_compiler_c/build/gmb run --project samples/modules/FolderResolution

# direct C99Native (complete runtime recipe, also for chart/db/AI)
tools/gmb_c99native_run.sh samples/modules/FolderResolution/AllFolders.bas o7
```

`check` prints `7 sorgenti del progetto, 8 moduli` (the two folders outside the root enter
from `importFolders`/`importFiles`). Expected output of `run`: one line per folder, with the
provenance of the module. Gate: `native/gmb_c99native/tests/c99_module_resolve_test.sh` (resolution)
and `native/gmb_compiler_c/tests/gmb_cli_project_test.sh` (CLI).

## Suite verification

```bash
# ALL the MiniTests: driver → C → cc -fsyntax-only (no link, no window)
native/gmb_compiler_c/build/gmb minitests            # readable summary, exit 0/2

# GATE: comparison with the versioned baseline, red only on regressions
tools/check_minitests.sh            # exit 0 ok · 2 regression · 1 broken setup
tools/check_minitests.sh --update   # regenerates the baseline (after a fix or a new MiniTest)
```

It is the single point: `tools/c99_compile_errors.sh` is now only an alias that delegates here. Gate of the
command: `native/gmb_compiler_c/tests/gmb_cli_minitests_test.sh`; baseline:
`native/gmb_compiler_c/tests/minitests_baseline.json` (35 KB, 84 known FAIL as of 2026-10-24).
A floor that goes down does not make the gate fail: it appears as `RISOLTO <file>`, after which
you regenerate the baseline with `--update`.
