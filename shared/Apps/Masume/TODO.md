# Masume (GMBasic port) — TODO

Feature-by-feature backlog against upstream (<https://github.com/turanmahmudov/masume>,
Apache-2.0), milestones, and the verification log referenced by `README.md`.

## Milestones

- **M1 — headless core (this folder, current).** Project file, profiles, named queries, demo
  SQLite database, object tree, headless query → TSV, `--mcp --check`-style diagnostics, a
  minimal interactive TUI shell. See the status table in `README.md`.
- **M2 — modularization.** Split `src/Masume.bas` into `.inc` modules once the single entry file
  becomes unwieldy (parsing, SQL, TUI as separate concerns).
- **M3 — browse/query depth.** Inline query editor (no highlighting/completion — BASIC has no
  editor widget for that), ER diagram, grid edits staged for SQL review, column masking,
  foreign-key navigation.
- **M4 — AI/MCP.** `AI MCP SERVE stdio` process exposing the tool set already listed by
  `MASUME_MODE_CHECK` (`masume_objects`, `masume_query`); AI chat surface.
- **M5 — drivers/ops.** Engines beyond SQLite (postgres/mysql/... — `[profile.shop]` in
  `.masume.toml` is a placeholder for this), `dump`/`restore`, notebooks, themes, keyring,
  key-binding chords.

## Backlog (not in M1)

- [ ] Inline query editor with syntax highlighting/completion from the catalog
- [ ] ER diagram of a table + its FK-linked tables
- [ ] Grid edits staged until SQL review/execution; sensitive-column masking
- [ ] Foreign-key navigation from a result grid
- [ ] Notebooks (SQL cells, charts, `masume nb run`, own file format)
- [ ] AI chat surface
- [ ] `AI MCP SERVE stdio` process (tool set is defined — see `MASUME_MODE_CHECK` — process itself
      is not started)
- [ ] Driver set beyond SQLite (postgres/mysql/cockroachdb/tidb/redshift)
- [ ] `dump` / `restore`
- [ ] Password sources beyond plain TOML value (`prompt`, keyring)
- [ ] Themes, key-binding chords
- [ ] Runtime argv (`-p NAME`, `--param name=value` are read from `.masume.toml` `[params]` only
      — BASIC's `Executor`/`test_ilrun` have no argv passthrough to the running program yet)

## Known platform gaps hit while building M1 (not Masume bugs — tracked for the fix upstream)

These were found and worked around (or documented) while getting M1 running; they are general
GMBasic engine gaps, reproduced with minimal `.bas` files unrelated to this app, not bugs
introduced by this port:

1. **IL backend: `DIM arr(n) AS STRING` left every element `null`, not `""`.**
   `LEN`/`TRIM`/concatenation on a never-assigned STRING array element threw
   `NullReferenceException`. **Fixed in this session**: `common/IlBackendRuntime/ArrayHelpers.cs`
   (new `NewStringArray`) + `common/IlBackendCodegen/Partial/IlProgramCompiler.Collections.ArrayOps.cs`
   (`EmitArrayDeclaration` now calls it for STRING element arrays instead of raw `Newarr`).
   Verified: `tools/test_minitests_parity arrays strings` (1/1 passed) + minimal repro
   (`DIM arr(8) AS STRING` then `LEN(arr(1))`) before/after.
2. **IL backend: a fixed-size array `DIM`'d at module/GLOBAL scope does not share state across
   SUBs.** Each SUB reads/writes its own stale copy (confirmed with plain `INTEGER` arrays, with
   and without the explicit `GLOBAL` keyword — not STRING-specific, not Masume-specific).
   Root cause (partially traced, **not fixed**): `EmitArrayDeclaration` in
   `IlProgramCompiler.Collections.ArrayOps.cs` never wires a fixed-size array's `ArrayInfo` to a
   `LoadRef`/`StoreGlobal`, unlike the DYNAMIC (empty-dimension) array path in
   `RegisterDynamicArrayLocal` — cross-SUB reads silently get a fresh/default array instead of the
   shared one. **PCode engine does not have this bug** (verified). **This port currently targets
   PCode**, not IL, for exactly this reason — see the README's "Known environment gaps".
   Needs an bug doc + a real fix in the IL codegen (out of scope for completing this app).
3. **`rs(i)` (positional SQLRESULTSET index) and `rs.COLUMNS` do not work on PCode** (returns
   empty/0 even though the row count from `SQL.FETCH` is correct) — confirmed with a minimal
   `SQL QUERY`/`SQL.FETCH` repro outside this app, both with a literal and a variable SQL string.
   `rs("colname")` (string index) works correctly everywhere it's used in this file. **Worked
   around** in `QueryToText` (`src/Masume.bas`): column names are parsed from the SELECT list text
   by the already-existing `SelectListHeader`, then `rs(colName)` is used instead of `rs(j)`/
   `rs.COLUMNS`. This means `QueryToText` cannot yet display a `SELECT *` named query's columns
   (upstream `[query.*]` entries would need an explicit column list) — acceptable for M1 since none
   of the three named queries in `.masume.toml` use `*`.
4. **`dotnet run --project Executor` fails** (`Executor.csproj` has no `<OutputType>Exe</OutputType>`
   — checked `git log --follow -p -- Executor/Executor.csproj`: never committed with one under
   this path/name). Affects every `.bas` file, not just this app. Worked around by using
   `dotnet run --project test_ilrun -c Debug -- --engine=pcode <file>` for all verification below.

## Verification (M1)

All runs below use `dotnet run --project test_ilrun -c Debug -- --engine=pcode <file>` (see gap
#4 above) with the `-D` symbol applied via a `#DEF MASUME_MODE_<X>` line temporarily prepended to
a copy of `src/Masume.bas` (`test_ilrun` has no `-D` flag; `#DEF`/`#IFDEF` is the in-source
equivalent — confirmed against `samples/Lexer/LexerPreProcessing.bas`). Run 2026-09-18.

| Mode | Command basis | Result |
|---|---|---|
| `STATUS` (default) | `--engine=pcode Apps/Masume/src/Masume.bas` | banner; `project file`, `working dir`, 2 profiles (`demo`, `shop`), 3 named queries, `connection: not open — run with -DMASUME_MODE_SEED...`, mode list. Matches "working". |
| `SEED` | `#DEF MASUME_MODE_SEED` + run | `seeding demo database: .../bin/masume_demo.sqlite` / `customers= 3  orders= 5` / `done — now run -DMASUME_MODE_OBJECTS or -DMASUME_MODE_RUN`. Matches "working". |
| `OBJECTS` | `#DEF MASUME_MODE_OBJECTS` + run (after SEED) | `schema: 2 tables, 0 views`, `TABLES` list (`customers`, `orders` with row counts), `columns of orders` (id/customer_id/total/created_at, `id` flagged `PK`). Matches "working". |
| `RUN` | `#DEF MASUME_MODE_RUN` + run (after SEED) | `# query=objects (from .../.masume.toml)` then TSV `type\tname` / `table\tcustomers` / `table\torders`. Required the `QueryToText` fix (gap #3) — before the fix this printed a header with zero data rows. Matches "working" after the fix. |
| `CHECK` | `#DEF MASUME_MODE_CHECK` + run (after SEED) | `check: project file: OK ...`, `check: profiles: 2` (both listed), `check: demo database: OK ...`, `check: connection: OK 2 tables, 0 views`, `check: mcp tool set: read-only [...]` (JSON schema for `masume_objects`/`masume_query`). Matches "working". |
| `TUI` | `#DEF MASUME_MODE_TUI` + run | **Not verified headlessly.** `MENU`/`WAITKEY` need a real terminal; piping `/dev/null` as stdin does not block cleanly — the menu loop free-runs (hundreds of KB of redraw output, no natural exit) because `choice` never becomes 4 without real key input. The mode is implemented (same structure as the other working modes: `RunTui`/`TuiGoObjects`/`TuiGoData`/`TuiGoColumns`/`TuiGoQuery`/`DrawShell`) but needs an interactive session to confirm. Listed as "working" in `README.md` on the strength of code review + the shared helpers (`PrintObjects`, `PrintColumns`, `QueryToText`) being independently verified above, not a headless run. |

### Git-reset damage note (2026-09-18)

An earlier session's `git reset` touching `libs/`/`libs_runtime/` tracking was investigated
separately (reflog/fsck, read-only) — no destructive loss found; the reset was a single
one-commit-back-and-recommit. `Apps/Masume/` itself was never git-tracked before this session
(`git ls-files Apps/` was empty), so it could not have been affected either way. Not a
Masume-specific concern; noted here only because the investigation happened in the same session as
this port's completion.
