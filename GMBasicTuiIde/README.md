# GMBasicTuiIde — standalone TUI IDE in BASIC (autonomous project)

A **complete** app written in **BASIC** (`.bas`): it is the *TUI version* of the IDE whose logic is in C
(`GMBasic/native/gmb_runtime_c/src/gmb_ide.c`). It compiles into a **native executable** (C99Native
path: `.bas` → C → binary) and does **not** run inside the REPL.

**Twin of `GMBasicIDE/`** (submodule, native macOS **graphical** version): same logic in C,
two different hosts — TUI here (compiled BASIC), GUI there (native app). The logic is **not** duplicated.

- **Origin:** original app of this project (no upstream to port).
- **License:** that of the GMBasic project.
- **Status:** promoted to **autonomous project of the monorepo** on 2026-10-31 (it was previously
  `shared/APPS/GMBasicIde`, renamed because the name collided with the graphical submodule).

## Why it is an autonomous project (and not a sample)

This IDE is the **footprint measuring bench**: it helps to understand **what a complex, compiled
BASIC program carries with it** — which native libraries it touches (FFI `gmb_gfx`), how many
resources it uses, how heavy the binary is and which dynamic dependencies it brings along. It is the
"heaviest" use case we have: 20+ `DECLARE … LIB "gmb_gfx"`, TUI controls, file I/O, FFI calls at every
frame. That is why it lives at the root as a project, not inside `shared/APPS/`.

### How it is measured (commands, not impressions)

The C99Native runner emits everything next to the entry file (`GMBasicTuiIde/src/bin/`):
`<base>_<suff>.c` (generated C), `<base>_<suff>` (binary), `<base>_<suff>.libs.txt`
(manifest of the required libraries), `libs.txt`, `Resources/`, `settings/`.

```sh
# 1) native compilation (CWD = monorepo root; the runner enters GMBasic/)
#    NB: the path is relative to GMBasic/ → `../GMBasicTuiIde/...`
GMB_TUI_SCRIPT=$'\e' bash tools/sh/gmb_c99native_run.sh ../GMBasicTuiIde/src/GMBasicTuiIde.bas

# 2) weight of the binary and of the generated C
ls -lh GMBasicTuiIde/src/bin/GMBasicTuiIde_out{,.c}

# 3) real dynamic dependencies (macOS; on Linux `ldd`)
otool -L GMBasicTuiIde/src/bin/GMBasicTuiIde_out

# 4) libraries that the runtime loads at RUN time (FFI/`dlopen`), with URL+sha256+size
cat GMBasicTuiIde/src/bin/GMBasicTuiIde_out.libs.txt
```

**Measurement of 2026-10-31** (osx-arm64, `-O1`, `C99Native`): binary **1,2 MB**, generated C **81 KB**,
binary dynamic dependencies `libSystem`/`libcurl`/`libz` + `@rpath/libSDL2{,_ttf}`; **10**
libraries loaded at runtime for **≈ 6,0 MB** (the heaviest `libgmb_gfx` 1,4 MB, then `libglib`
1,3 MB, `libharfbuzz` 1,2 MB, `libSDL2` 0,5 MB, …). Update this line when measuring again.

## Project layout

```
GMBasicTuiIde/
  README.md                    <- this file
  TODO.md                      <- remaining work
  src/GMBasicTuiIde.bas        <- entry point (SUB Main)
  work/                        <- runtime output (generated C + binary), gitignored
```

## How it works (a single logic, many hosts)

- The **logic** (outline, completion, go-to-def, find-refs, lint, rename, snippet, make) is **in C**
  (`gmb_ide`), exposed to BASIC by **FFI-friendly** wrappers `gmb_idef_*`
  (`GMBasic/native/gmb_runtime_c/src/gmb_ide.c`).
- This `.bas` drives it via **FFI** (`DECLARE … LIB "gmb_gfx"`) and displays it with the **TUI controls**
  (registry `gmb_tui_*` + **non-blocking** frame `gmb_ui_frame`). **No new builtin/TokenKind.**
- `gmb_gfx` is **our internal** lib: it compiles into **`GMBasic/build/libs/<rid>/`** and is
  published to **`~/.gmbasic/libs/<rid>/`** (vault: `Knowledge/Librerie-e-Pacchetti.md`). The FFI
  finds it automatically: the loader looks in `libs` → `shared/libs` → `build/libs/<rid>` and then the cache.

## UI layout and keys

```
+----------------------------------------------------------+
| File: [path]                                             |
| [ tab1 | tab2 | ... ]                                    |
| CodeArea (source)                  | ListBox (outline)   |
|                                     |                     |
+----------------------------------------------------------+
| StatusBar: messages / command result                     |
+----------------------------------------------------------+
```

| Key | Action |
|---|---|
| `F1`  | Help |
| `F2`  | Open the file indicated in the "File:" field |
| `F3`  | Save the current buffer to the path |
| `F4`  | Outline (symbol analysis) |
| `F5`  | Completion (prefix "PR") |
| `F6`  | Lint (squiggle in the gutter) |
| `F7`  | Go-to-definition (symbol selected in the outline) |
| `F8`  | Find-references (selected symbol) |
| `F9`  | Make (writes the normalized document to `<file>.build.bas`) |
| `F10` | New tab |
| `ESC` | Quit |

## How to run

From the **monorepo root** (`GMBasicMonoRepo/`). The app is **interactive**
(`' TAG: interactive`): without a TTY one uses `GMB_TUI_SCRIPT` to script the keys, otherwise
it closes immediately.

```sh
# standalone compilation + execution (C99Native)
#   the .bas path is relative to GMBasic/ (the runner enters there)
GMB_TUI_SCRIPT=$'\e' bash tools/sh/gmb_c99native_run.sh ../GMBasicTuiIde/src/GMBasicTuiIde.bas

# dev loop on IL (JIT)
tools/sh/harness.sh il GMBasicTuiIde/src/GMBasicTuiIde.bas

# scripted execution (ESC to quit)
GMB_TUI_SCRIPT=$'\e' tools/sh/harness.sh il GMBasicTuiIde/src/GMBasicTuiIde.bas
```

Prerequisites: native build present (`GMBasic/native/gmb_vm_c/build`) and native libraries in the cache
(`~/.gmbasic/libs/<rid>/`, from `gmb install --runtime-only`).

## Documentation

Manuals and notes in the monorepo vault (`01-Projects/GMBasicMonoRepo/`), not here:
`Todo/(open) - IDE-TUI-BASIC-e-standalone-e-VSCode-LSP.md` (A/B/C plan: IDE in BASIC, IDE in C,
LSP for VS Code).
