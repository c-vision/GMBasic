# APPS — complete applications in GMBasic

This folder hosts **complete applications written in GMBasic**, obtained by porting existing
applications written in other languages (Go, Python, C#, Rust, ...).

Each application is **self-contained**: its folder holds every file needed to compile and run
the BASIC program (`*.bas` entry point, app-local `*.inc` modules, data/config files, docs).

## Layout

```
APPS/
  README.md                  <- this file (conventions, app index)
  <AppName>/                 <- one dedicated folder per application
    README.md                <- REQUIRED: what it is, upstream links, license/attribution,
                               scope, how to build/run, status
    TODO.md                  <- REQUIRED: complete port todo (upstream feature-by-feature)
    src/                     <- BASIC sources
      <AppName>.bas          <- entry point (SUB Main)
      <AppName>.*.inc        <- optional app-local modules (NAMESPACE ... + IMPORT <Namespace>)
    fixtures/                <- OPTIONAL: versioned input data (never written at runtime)
    work/                    <- runtime output, generated — NEVER versioned (see below)
```

Shared, reusable code does **not** live here — it lives in the two dedicated root folders:

| What | Where | Notes |
|---|---|---|
| **Reusable includes** (module source for other GMBasic programs: `.inc` files with `NAMESPACE` + `PUBLIC`) | `<repo>/include/` | Auto-discovered: the compiler walks up from the app folder until it finds `<repo>/include` (marker file `include/HttpController.inc`) and adds it as the lowest-priority fallback. A same-named file inside the app **shadows** the root one. Used with `IMPORT <Namespace>` — same mechanism as `include/Rbac.inc` / `include/Validation.inc`. |
| **Reusable libraries** (bigger, standalone library code: BASIC modules, plus any non-BASIC asset a library needs) | `<repo>/libs/` | WARNING: **currently gitignored** — `.gitignore:177` ignores `/libs/` (it is used as the local C/C++ build-dependency cache, e.g. `libs/mlx-deps`, ~130 MB, regenerable). Placing a *versioned* BASIC library here needs an explicit `.gitignore` exception (e.g. `/libs/` + `!/libs/gmb/`), and for the compiler to find it the folder must be listed in `importFolders` of `config/system.settings.json`. Do not silently `git add -f` a library: it stays untracked for everyone else. |

## How the compiler finds an app's files

The compiler treats **the directory of the entry `.bas` file** as the *project root* and
discovers, recursively, every `*.bas` and `*.inc` below it (`Executor/Orchestrator.cs`,
`DiscoverSourceFiles`). It then tokenizes `IMPORT`-driven, starting from the entry file: to add
a module you write `IMPORT <Namespace>` and the module file declares `NAMESPACE <Namespace>`.

There is **no textual `INCLUDE` directive** in the language: modularity is `IMPORT` + `.inc`
modules (optionally packages: `gmb-package.json` / `REQUIRE`, see `REPL/CliPackages.cs`).

Consequence: keep one app per folder and keep the entry file at the app level you want scanned —
a stray `SUB Main` in a sibling file of the same project root is a link-time risk.

## Output conventions

* Generated data (databases, exports, logs, dumps) goes to **`work/` inside the app folder** —
  e.g. `APPS/Masume/work/masume.db`. `**/work/` is gitignored, so runtime artifacts never
  pollute the repository or the app's tracked contents.
* Never write generated files into the repo root (see `AGENTS.md` §9) — the root is not a
  scratch space.
* `AGENTS.md` §11 (4 categories of files) applies to apps too: fixtures are *versioned inputs*,
  `work/` is *generated output*.

### Data-file extensions and git

`.gitignore` ignores `*.db`, `*.dbf`, `*.dat`, `*.isdb`, `*.ntx`, `*.fpt` repo-wide, and
`**/work/` hides anything generated. Consequence: a **fixture** database cannot be committed
with those extensions — use `.sqlite` / `.sqlite3` (not ignored), or generate the fixture at
runtime with a seed script (`git add -f` is a last resort, and never for generated output).

## How to run an app

```sh
# 1) IL backend (JIT, dev loop)
dotnet run --project Executor -- APPS/<AppName>/src/<AppName>.bas

# 2) IL backend, NativeAOT executable (release)
dotnet run --project REPL -- -n APPS/<AppName>/src/<AppName>.bas

# 3) VM/C (external `gmb_vm_host` process)
dotnet run --project tests/GMBASIC.Conformance -- vmtest APPS/<AppName>/src/<AppName>.bas

# 4) C99Native (C frontend + emitter → `cc` → run; CWD = GMBasic/)
(cd GMBasic && bash tools/gmb_c99native_run.sh ../shared/APPS/<AppName>/src/<AppName>.bas)

# Compile-time symbols are injected with -D<SYMBOL> and consumed by #IFDEF in the source
dotnet run --project Executor -- -D<APPNAME>_MODE_<MODE> APPS/<AppName>/src/<AppName>.bas
```

Note: BASIC currently has **no runtime argv / environment access** (no `ARGV`, no `ENV()`
builtin — verified against `common/Registry/stdlib_tokens.csv` and `REPL/Arguments.cs`). Apps
therefore select their behaviour through `#IFDEF` compile-time symbols, project/config files,
and interactive UI. That gap is tracked in the vault
(`01-Projects/GMBasic/Todo/(open) - CLI-Args-e-Env-per-Applicazioni-BASIC.md`).

## Rules for a port (non-negotiable)

1. **No stubs.** Every ported feature either works or is explicitly documented as not
   implemented (with a clear runtime error / TODO entry). Silent no-ops are forbidden
   (`Startup-Protocol.md` — "DIVIETO ASSOLUTO DI CREARE STUB").
2. **Canonical syntax.** Commands never take parentheses; functions always do; the namespace is
   the domain name (`AGENTS.md` §1–§2). Same-word read/write pairs must be split into a command
   (write) and a function (read).
3. **Docs stay in the vault.** User manuals, guides, analyses, plans → Obsidian
   `01-Projects/GMBasic/` (`Guide/`, `Knowledge/`, `Piani/`, ...). The repo `docs/` folder is
   gitignored and off-limits. The app's own `README.md` / `TODO.md` are part of the app.
4. **Licence compliance.** Every port keeps the upstream licence and attribution in its
   `README.md` (upstream repository URL, licence name, what was reused and what was rewritten).
5. **Verified, not assumed.** Say how the app was executed and what output was observed. Run the
   narrowest command that exercises the change (no massive test sweeps without permission,
   `AGENTS.md` §0).

## App index

| App | Upstream | Language | Licence | Status |
|---|---|---|---|---|
| [`Masume`](Masume/README.md) | [turanmahmudov/masume](https://github.com/turanmahmudov/masume) — terminal database client with AI chat and an MCP server | Go | Apache-2.0 | M1 headless core (status / seed / objects / run / check) — in progress |
| [`GMBasicIde`](GMBasicIde/README.md) | — (originale) | BASIC | GMBasic | v1.0 — IDE TUI (multi-tab, I/O file, outline/completion/lint/goto/make); verificato su IL + standalone C99Native |

A port starts only when its folder has `README.md` + `TODO.md` and an entry `.bas` that compiles
and runs.

