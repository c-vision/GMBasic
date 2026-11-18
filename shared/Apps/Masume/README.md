# Masume (GMBasic port)

**Masume** is a keyboard-first **terminal database client** with AI chat and an MCP server. This
folder holds the **GMBasic port** of that application.

## Upstream (source of the port)

| What | Link |
|---|---|
| Repository | <https://github.com/turanmahmudov/masume> |
| Default branch | `master` (85 commits at the time of this port) |
| Licence | **Apache License 2.0** — <https://github.com/turanmahmudov/masume/blob/master/LICENSE> |
| README | <https://github.com/turanmahmudov/masume/blob/master/README.md> |
| Docs — usage (navigation, editing, data transfer) | <https://github.com/turanmahmudov/masume/blob/master/docs/usage.md> |
| Docs — engines (postgres, mysql, sqlite, cockroachdb, tidb, redshift, ...) | <https://github.com/turanmahmudov/masume/blob/master/docs/engines.md> |
| Docs — configuration (profiles, settings, password sources) | <https://github.com/turanmahmudov/masume/blob/master/docs/configuration.md> |
| Docs — hooks/keys (default bindings, scopes) | <https://github.com/turanmahmudov/masume/blob/master/docs/keys.md> |
| Docs — notebooks (cells, charts, file format, `masume nb run`) | <https://github.com/turanmahmudov/masume/blob/master/docs/notebooks.md> |
| Docs — AI chat (providers, tools, what is sent to the provider) | <https://github.com/turanmahmudov/masume/blob/master/docs/ai.md> |
| Docs — MCP server (tools, limits, confirming a write) | <https://github.com/turanmahmudov/masume/blob/master/docs/mcp.md> |
| Docs — headless mode (`masume run`, formats, exit codes, dump/restore) | <https://github.com/turanmahmudov/masume/blob/master/docs/headless.md> |
| Docs — themes | <https://github.com/turanmahmudov/masume/blob/master/docs/themes.md> |
| Docs — security (storage, data sharing, protection limits) | <https://github.com/turanmahmudov/masume/blob/master/SECURITY.md> |
| Contributing | <https://github.com/turanmahmudov/masume/blob/master/CONTRIBUTING.md> |
| Example configuration (full reference) | <https://github.com/turanmahmudov/masume/blob/master/config.example.toml> |
| Compose (container databases for local testing) | <https://github.com/turanmahmudov/masume/blob/master/compose.yaml> |

## What the upstream program is

A single-binary TUI client (`masume`) for relational engines. Its surfaces, as documented
upstream:

* **Browse** — object tree of the database objects of the engine; table views showing data,
  columns, indexes, constraints, DDL and query plans.
* **Diagram** — ER diagram of a table and the tables its foreign keys link to.
* **Query** — editor with syntax highlighting and completion from the database catalog, local
  and supported server-side pre-execution diagnostics.
* **Results** — sort, filter, follow a foreign key, freeze a column; grid edits stay staged
  until SQL review and execution; masking of sensitive columns for display.
* **Notebooks** — SQL cells with charts, run policy, own file format, `masume nb run`.
* **AI chat** — providers, tools, explicit statement of what is sent to the provider.
* **MCP server** — `masume --mcp [--profile=NAME|--check]`, tool limits, write confirmation.
* **Headless** — `masume run` (JSON/CSV), `dump` / `restore`, params (`--param day=...`),
  profiles (`-p`), exit codes.
* **Project file** — `.masume.toml` with shared `[profile.*]` and `[query.*]`, discovered in or
  above the working directory.
* **Configuration** — profiles (engine/host/port/database/user/auth/env/mode), password sources
  (`prompt`, keyring), themes, key bindings/chords.

## GMBasic port — scope, status, licence/attribution

**Licence.** Upstream is Apache-2.0. This port is a **clean rewrite in GMBasic**: it does *not*
copy Go source code. What is carried over is the *observable behaviour* — command surface, the
`.masume.toml` project-file layout, headless output shape and the interaction model — so the
upstream README and `docs/` remain the reference documentation; any future reuse of upstream code
must keep the Apache-2.0 `LICENSE`/NOTICE obligations.

**Status: M1 (headless core) — implemented in this folder.** Full feature-by-feature backlog and
milestones: [`TODO.md`](TODO.md).

| M1 mode (`-D` symbol) | Ported surface | State |
|---|---|---|
| `MASUME_MODE_STATUS` (default) | banner + startup self-check: project file, profiles, named queries, database connectivity | working |
| `MASUME_MODE_SEED` | demo database (SQLite via `SQL OPEN`) into `bin/` — customers/orders with a FK | working |
| `MASUME_MODE_OBJECTS` | object tree: tables + views with row counts, columns of a table | working |
| `MASUME_MODE_RUN` | `masume run` equivalent: named/default query → TSV on stdout, `--param`-style substitution | working |
| `MASUME_MODE_CHECK` | `masume --mcp --check`-style diagnostics: allowed profiles, read-only enforcement of the MCP tool set | working |
| `MASUME_MODE_TUI` | interactive: `MENU` action menu + `TBROWSE` object tree / data grid + `WINDOW` shell | implemented — needs a real terminal to verify (see `TODO.md`) |

Not in M1 (see `TODO.md`): inline editor with highlighting/completion, ER diagram, grid editing
with staged SQL review, column masking, foreign-key navigation, notebooks with charts, AI chat,
MCP server process, driver set beyond SQLite, keyring, themes, chords, `dump`/`restore`.

## Files

| Path | Role |
|---|---|
| `src/Masume.bas` | entry point (`SUB Main`), mode dispatch, banner, self-check. **Everything the app needs to compile and run is this file.** |
| `.masume.toml` | example project file (upstream-compatible layout): `[profile.*]` + `[query.*]` + `[settings]` + `[llm]`, read by the app from the working directory upward |
| `README.md` | this file |
| `TODO.md` | complete port backlog + milestones |
| `bin/` | runtime output (demo database, exports) — generated, gitignored |

App-local modules (`*.inc`, `NAMESPACE ...`) are added when the single entry file becomes
unwieldy (see `TODO.md` §M2 "modularization"). **Reusable** modules go to `<repo>/include/` and
reusable libraries to `<repo>/libs/` — see [`../README.md`](../README.md).

### LLM: the model is configured in the project file (not in the code)

The app uses an LLM (chat + MCP tools) and **does not hardcode the model**: it reads it from `.masume.toml`,
section `[llm]`, at boot (`LlmActivate` → `AI SET ACTIVE <capability>, <model>`).

```toml
[llm]
model = ""                 # capability alias/provider (e.g. "ds4mlx"); "" = provider already active
capability = "completion"  # default
```

- **empty `model`** → the `%CHAT`/`completion` provider already active stays (language default).
- **unregistered alias** → the app **fails with an explicit error** (`Provider 'x' is not
  registered for role 'completion'`), never a silent fallback to another model.
- The value is shown in `MASUME_MODE_STATUS` (`llm model : ...`), so it can be verified without
  calling the model.

## How to build and run

From the **monorepo ROOT** (`GMBasicMonoRepo/`):

```sh
# dev loop — one file, one leg (PCode = verified engine of this port; see IL caveat below)
tools/sh/harness.sh pcode shared/Apps/Masume/src/Masume.bas
tools/sh/harness.sh il    shared/Apps/Masume/src/Masume.bas
tools/sh/harness.sh vmc   shared/Apps/Masume/src/Masume.bas

# release: NativeAOT executable
REPL/build/gmbrepl shared/Apps/Masume/src/Masume.bas

# C99Native (C frontend + emitter → cc → execution)
bash tools/sh/gmb_c99native_run.sh ../shared/Apps/Masume/src/Masume.bas   # relative to GMBasic/
```

The **modes** (`_SEED`, `_OBJECTS`, `_RUN`, `_CHECK`, `_TUI`) are **compile-time** symbols (`-D` +
`#IFDEF`): today **no monorepo runner passes `-D`** (`test_ilrun`/`test_pcode`/`test_vmrun`
have no such option; `REPL -D` applies to REPL directives, not to the execution of a file).
The mode is tried by adding **`#DEF MASUME_MODE_<X>`** at the top of the file (in-source
equivalent of `-D`, as documented in `TODO.md` §Verification). CLI support is tracked in the gap
`Todo/(open) - CLI-Args-e-Env-per-Applicazioni-BASIC` of the monorepo vault.

A variable name that is a BASIC keyword (`ITEMS`, `INTO`, `AT`, `NAME`, `TEXT`, …) must be
renamed: `items` would collide with the `ITEMS` clause of the control commands — see
`Hard-Lessons/ITEMS-Keyword-Collision-Variabile` in the monorepo vault (in `RunTui` the array is
called `menuItems`).

Run from `shared/Apps/Masume/` (or from one of its subfolders), so the `.masume.toml` project file
is found: the app searches in the working directory and its ancestors.

**Environment note:** `GMBasicSharp/Executor` is a **Library** (`OutputType` not `Exe`), so
`dotnet run --project Executor` is not a valid path — use the runners above. The **PCode**
engine is this port's verified engine; **IL** has a pre-existing and general bug: a fixed-size
array declared at module/GLOBAL level does not share state across SUBs (confirmed with
a minimal repro unrelated to this app — every SUB sees its own stale/default copy) — this port's
`g_prof*`/`g_query*`/`g_param*` global arrays hit it directly, so **IL is not currently usable for
this app**. Tracked as a new bug (see `TODO.md`).

## Recorded verification

The "Verification" section of [`TODO.md`](TODO.md) lists, for every verified claim about this
port, the exact command, the backend leg and the observed output.