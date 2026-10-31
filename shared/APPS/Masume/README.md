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
| `MASUME_MODE_SEED` | demo database (SQLite via `SQL OPEN`) into `work/` — customers/orders with a FK | working |
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
| `.masume.toml` | example project file (upstream-compatible layout): `[profile.*]` + `[query.*]`, read by the app from the working directory upward |
| `README.md` | this file |
| `TODO.md` | complete port backlog + milestones |
| `work/` | runtime output (demo database, exports) — generated, gitignored |

App-local modules (`*.inc`, `NAMESPACE ...`) are added when the single entry file becomes
unwieldy (see `TODO.md` §M2 "modularization"). **Reusable** modules go to `<repo>/include/` and
reusable libraries to `<repo>/libs/` — see [`../README.md`](../README.md).

## How to build and run

```sh
# fast dev loop (IL backend, JIT)
dotnet run --project Executor -- APPS/Masume/src/Masume.bas

# the demo database must exist before objects/run (once, or after deleting work/)
dotnet run --project Executor -- -DMASUME_MODE_SEED APPS/Masume/src/Masume.bas

# object tree, named query, diagnostics
dotnet run --project Executor -- -DMASUME_MODE_OBJECTS APPS/Masume/src/Masume.bas
dotnet run --project Executor -- -DMASUME_MODE_RUN APPS/Masume/src/Masume.bas
dotnet run --project Executor -- -DMASUME_MODE_CHECK APPS/Masume/src/Masume.bas

# interactive TUI
dotnet run --project Executor -- -DMASUME_MODE_TUI APPS/Masume/src/Masume.bas

# other backends (parity / release verification)
dotnet run --project REPL -- -n APPS/Masume/src/Masume.bas
dotnet run --project tests/GMBASIC.Conformance -- vmtest APPS/Masume/src/Masume.bas
(cd GMBasic && bash tools/gmb_c99native_run.sh ../shared/APPS/Masume/src/Masume.bas)
```

Run it from `APPS/Masume/` (or any directory below it) so the project file `.masume.toml` is
found: the app searches the working directory and its ancestors.

**Known environment gaps (2026-09-18, see `TODO.md` §M1 "Verification"):**
`dotnet run --project Executor` currently fails in this checkout (`Executor.csproj` has no
`<OutputType>Exe</OutputType>` — unrelated to this port, affects every `.bas` file). Until that is
fixed, use `dotnet run --project test_ilrun -c Debug -- --engine=pcode APPS/Masume/src/Masume.bas`
(`--engine=il` also compiles/runs, but see the IL-backend caveat below). The **PCode** engine is
this port's verified engine; the **IL** engine has a pre-existing, general codegen bug where a
fixed-size array declared at module/GLOBAL scope does not share state across SUBs (confirmed with
a minimal repro unrelated to this app — every SUB sees its own stale/default copy) — this port's
`g_prof*`/`g_query*`/`g_param*` global arrays hit it directly, so **IL is not currently usable for
this app**. Tracked as a new AiWiki bug (see `TODO.md`).

## Recorded verification

The "Verification" section of [`TODO.md`](TODO.md) lists, for every verified claim about this
port, the exact command, the backend leg and the observed output.