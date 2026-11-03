# Apps — complete applications in GMBasic

This folder hosts **complete applications written in GMBasic**, obtained by porting existing
applications written in other languages (Go, Python, C#, Rust, ...).

Each application is **self-contained**: its folder holds every file needed to compile and run
the BASIC program (`*.bas` entry point, app-local `*.inc` modules, data/config files, docs).

## Layout

```
Apps/
  README.md                  <- this file (conventions, app index)
  <AppName>/                 <- one dedicated folder per application
    README.md                <- REQUIRED: what it is, upstream links, license/attribution,
                               scope, how to build/run, status
    TODO.md                  <- REQUIRED: complete port todo (upstream feature-by-feature)
    src/                     <- BASIC sources
      <AppName>.bas          <- entry point (SUB Main)
      <AppName>.*.inc        <- optional app-local modules (NAMESPACE ... + IMPORT <Namespace>)
    fixtures/                <- OPTIONAL: versioned input data (never written at runtime)
    bin/                     <- runtime output + compiled artifacts, generated — NEVER versioned
```

> **Why `bin/` and not `work/`**: `work/` is **reserved for samples** (`shared/samples/**/work/`)
> and holds **only data artifacts** produced by a BASIC program for verification (AGENTS §11.1
> cat.4). Complete apps are not samples: their output and binaries go into **`bin/`**, the
> compiler's default output folder (AGENTS §9.6), which is already gitignored and must be emptied
> after every run.

Shared, reusable code does **not** live here — it lives in the two dedicated root folders:

| What | Where | Notes |
|---|---|---|
| **Reusable includes** (module source for other GMBasic programs: `.inc` files with `NAMESPACE` + `PUBLIC`) | **`<monorepo>/shared/include/`** | Reached through `compiler.searchFolders` in `shared/settings/system.settings.json`: `include`, `Controls`, `controls`, `{install}/../shared/include`, `{install}/../shared/Controls`, `{install}/../shared/libs/*`, `{install}/libs/*` (`{install}` = `GMBASIC_ROOT`, that is `GMBasic/` in the monorepo). A file inside the app **shadows** the global one by name. Used with `IMPORT <Namespace>` — same mechanism as `shared/include/Rbac.inc` / `Validation.inc`. |
| **Reusable libraries** (bigger, standalone library code: BASIC modules + header/exports) | **`<monorepo>/shared/libs/<nome>/`** | The libraries are versioned **projects** (`src/`, `include/`, `build.sh`, `samples/`, `<nome>.inc`). They are **not** a dependency cache: the **binaries** are ignored (`shared/.gitignore`: `libs/*/lib*.{dylib,a,so,dll}`, `libs/*.{dylib,a,so,dll}`) and are produced/published with `tools/sh/gmb_lib.sh build|check|publish`. The old `/libs/` cache (~130 MB) **no longer exists**. For a new library: `shared/libs/<nome>/` + export in `shared/include/<nome>.inc` (rule: vault `Knowledge/`, `Librerie-e-Pacchetti`). |

## How the compiler finds an app's files

The compiler treats **the directory of the entry `.bas` file** as the *project root* and
discovers, recursively, every `*.bas` and `*.inc` below it (`GMBasicSharp/Executor/Orchestrator.cs`,
`DiscoverSourceFiles`; C side: `GMBasic/native/gmb_compiler_c/frontend/src/gmb_inc_resolve.c`,
same semantics). Then it resolves the `IMPORT`s starting from the entry file.

**Where it looks for files** (in order, the app's files **shadow** the global ones by name):

1. **recursive** scan of `*.bas`/`*.inc` in the app folder;
2. `importFolders` + `importFiles` from the settings (`shared/settings/system.settings.json` +
   `app.settings.json`), resolved both against the project and against the install;
3. `compiler.searchFolders` (same settings): default `include`, `Controls`, `controls`,
   `{install}/../shared/include`, `{install}/../shared/Controls`, `{install}/../shared/libs/*`,
   `{install}/libs/*` — where `{install}` is the install root (`GMBASIC_ROOT`, in the monorepo
   `GMBasic/`). There is no file-marker: the list is **configuration**, not a walk up from the
   app folder.

There is **no textual `INCLUDE` directive** in the language: modularity is `IMPORT` + `.inc`
modules (optionally packages: `gmb-package.json` / `REQUIRE`, see `GMBasic/native/gmbadmin/gmbadminpkg.c`).

Consequence: keep one app per folder and keep the entry file at the app level you want scanned —
a stray `SUB Main` in a sibling file of the same project root is a link-time risk.

## Output conventions

* Generated data (databases, exports, logs, dumps) goes to **`bin/` inside the app folder** —
  e.g. `Apps/Masume/bin/masume.sqlite`. `**/bin/` is gitignored, so runtime artifacts never
  pollute the repository or the app's tracked contents.
* **`work/` is not used in apps**: it exists only under `shared/samples/**/work/` (data artifacts
  of the samples). See the note in §Layout.
* Never write generated files into the repo root (see `AGENTS.md` §9) — the root is not a
  scratch space.
* `AGENTS.md` §11 (4 categories of files) applies to apps too: fixtures are *versioned inputs*,
  `bin/` is *generated output*.

### Data-file extensions and git

`.gitignore` ignores `*.db`, `*.dbf`, `*.dat`, `*.isdb`, `*.ntx`, `*.fpt` repo-wide, and
`**/bin/` hides anything generated. Consequence: a **fixture** database cannot be committed
with those extensions — use `.sqlite` / `.sqlite3` (not ignored), or generate the fixture at
runtime with a seed script (`git add -f` is a last resort, and never for generated output).

## How to run an app

**All commands are launched from the monorepo ROOT** (`GMBasicMonoRepo/`); the app paths are
therefore `shared/Apps/<AppName>/src/<AppName>.bas`.

```sh
# Dev loop — one file, one leg
tools/sh/harness.sh il    shared/Apps/<AppName>/src/<AppName>.bas
tools/sh/harness.sh pcode shared/Apps/<AppName>/src/<AppName>.bas
tools/sh/harness.sh vmc   shared/Apps/<AppName>/src/<AppName>.bas

# Release: NativeAOT executable (REPL is the only executable project)
REPL/build/gmb_repl shared/Apps/<AppName>/src/<AppName>.bas

# C99Native (C frontend + emitter → `cc` → execution)
bash tools/sh/gmb_c99native_run.sh ../shared/Apps/<AppName>/src/<AppName>.bas   # path relative to GMBasic/
```

> [!warning] **`GMBasicSharp/Executor` is NOT executable** (`OutputType=Library`): `dotnet run
> --project GMBasicSharp/Executor` fails. The `tools/runners/` runners (`test_ilrun`,
> `test_pcode`, `test_vmrun`) are the only execution path for a `.bas`.
>
> **Compile-time symbols (`-D`)**: **no runner passes them** — `test_ilrun`/`test_pcode`/`test_vmrun`
> have no such option and `REPL -D` applies to REPL directives, not to the execution of a file.
> To try a different mode you add the line `#DEF <SIMBOLO>` at the top of the file (in-source
> equivalent of `-D`, verified) — CLI support is tracked in the gap
> `Todo/(open) - CLI-Args-e-Env-per-Applicazioni-BASIC` (monorepo vault).

The per-app scripts live in the app folder (`ServiceConsole/console_tui.sh`,
`console_gui.sh`) and set the working directory themselves (CWD = `shared/`, so the app finds its
own config file); they resolve the tools from the monorepo root.

Note: BASIC currently has **no runtime argv / environment access** (no `ARGV`, no `ENV$()`
builtin — verified on `shared/spec/registry/surface.csv`; `REPL/Arguments.cs` interprets only
the tool's CLI). Apps therefore select their behaviour through `#IFDEF` compile-time symbols
(`-D<APP>_MODE_<MODE>`), project/config files, and interactive UI. The gap is tracked in the vault:
`01-Projects/GMBasicMonoRepo/Todo/(open) - CLI-Args-e-Env-per-Applicazioni-BASIC.md`.

## Rules for a port (non-negotiable)

1. **No stubs.** Every ported feature either works or is explicitly documented as not
   implemented (with a clear runtime error / TODO entry). Silent no-ops are forbidden
   (`Startup-Protocol.md` — "DIVIETO ASSOLUTO DI CREARE STUB").
2. **Canonical syntax.** Commands never take parentheses; functions always do; the namespace is
   the domain name (`AGENTS.md` §1–§2). Same-word read/write pairs must be split into a command
   (write) and a function (read).
3. **Docs stay in the vault.** User manuals, guides, analyses, plans → Obsidian
   `01-Projects/GMBasicMonoRepo/` (`Guide/`, `Knowledge/`, `Piani/`, ...). The repo `docs/` folder is
   gitignored and off-limits. The app's own `README.md` / `TODO.md` are part of the app.
4. **Licence compliance.** Every port keeps the upstream licence and attribution in its
   `README.md` (upstream repository URL, licence name, what was reused and what was rewritten).
5. **Verified, not assumed.** Say how the app was executed and what output was observed. Run the
   narrowest command that exercises the change (no massive test sweeps without permission,
   `AGENTS.md` §0).
6. **LLM: the model is configured in the app's settings** (rule, 2026-11-01). Every app that
   **exists or will exist** and uses an LLM model **must** allow specifying it in its **own**
   settings (project file/`.cfg`/`app.settings.json` of the app), **never hardcoded** in the `.bas`.
   Convention: `[llm]` section with `model = "<alias/provider>"` (+ optional `capability`);
   empty `model` = provider already active (language default); unregistered alias = **explicit
   error** (no silent fallback). Live reference: `Masume` (`.masume.toml` §`[llm]`,
   `LlmActivate`). Reason: change model (local/remote, quantized, other) without recompiling
   and without touching the app's code.

## App index

| App | Upstream | Language | Licence | Status |
|---|---|---|---|---|
| [`Masume`](Masume/README.md) | [turanmahmudov/masume](https://github.com/turanmahmudov/masume) — terminal database client with AI chat and an MCP server | Go | Apache-2.0 | M1 headless core (status / seed / objects / run / check) — in progress; **PCode verified**; C99Native does not compile (bug open in the vault: `Bugs/(open) - Masume-C99Native-Emissione-DoubleToString`) |
| [`DigUp`](DigUp/README.md) | [ARahim3/DigUp](https://github.com/ARahim3/DigUp) — semantic file search (EmbeddingGemma 2 via llama.cpp) | Swift/Tauri | MIT | M1 headless: SQLite index (AI RAG) + hybrid search over text/documents/code; **verified on IL/PCode/VM/C/C99Native**; media in TODO |
| [`RuleFlow`](RuleFlow/README.md) | [RuleFlow-OSS/RuleFlow](https://github.com/RuleFlow-OSS/RuleFlow) — rewrite systems (SSS/ECA) with causal graph and metrics | Python | MIT | M1 headless: FlowLang parser (`->` `-->` `>` `><`), events with delta, **causal DAG** (nodes/edges/density/max path); **verified on IL/PCode/VM/C/C99Native**; parallelism/multiway in TODO |
| [`ServiceConsole`](ServiceConsole/README.md) | — (original) | BASIC | GMBasic | test bench for the polymorphic **daemon + GUI** (in-language HTTP, tray, console TUI; scripts `console_tui.sh`/`console_gui.sh`) |

> **No longer here**: the TUI IDE (formerly `Apps/GMBasicIde`, now `GMBasicTuiIde`) is **not in this
> repository**: it is developed locally and excluded until complete (`.gitignore`), together with the
> GUI IDE (`GMBasicIDE/`). Reason it will be published as an autonomous project when ready: it is the
> measurement bench for the footprint of a complex, compiled BASIC program.

A port starts only when its folder has `README.md` + `TODO.md` and an entry `.bas` that compiles
and runs.

