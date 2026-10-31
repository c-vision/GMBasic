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

### LLM: il modello si configura nel project file (non nel codice)

L'app usa un LLM (chat + MCP tools) e **non hardcoda il modello**: lo legge da `.masume.toml`,
sezione `[llm]`, al boot (`LlmActivate` → `AI SET ACTIVE <capability>, <model>`).

```toml
[llm]
model = ""                 # alias/provider della capability (es. "ds4mlx"); "" = provider già attivo
capability = "completion"  # default
```

- **`model` vuoto** → resta il provider `%CHAT`/`completion` già attivo (default del linguaggio).
- **alias non registrato** → l'app **fallisce con errore esplicito** (`Provider 'x' is not
  registered for role 'completion'`), mai un fallback silenzioso a un altro modello.
- Il valore è mostrato in `MASUME_MODE_STATUS` (`llm model : ...`), così si verifica senza
  chiamare il modello.

## How to build and run

Dalla **ROOT del monorepo** (`GMBasicMonoRepo/`):

```sh
# dev loop — un file, una gamba (PCode = motore verificato di questo port; vedi caveat IL sotto)
tools/sh/harness.sh pcode shared/APPS/Masume/src/Masume.bas
tools/sh/harness.sh il    shared/APPS/Masume/src/Masume.bas
tools/sh/harness.sh vmc   shared/APPS/Masume/src/Masume.bas

# release: eseguibile NativeAOT
dotnet run --project GMBasicSharp/REPL -- -n shared/APPS/Masume/src/Masume.bas

# C99Native (frontend C + emitter → cc → esecuzione)
bash tools/sh/gmb_c99native_run.sh ../shared/APPS/Masume/src/Masume.bas   # relativo a GMBasic/
```

I **modi** (`_SEED`, `_OBJECTS`, `_RUN`, `_CHECK`, `_TUI`) sono simboli **compile-time** (`-D` +
`#IFDEF`): oggi **nessun runner del monorepo passa `-D`** (`test_ilrun`/`test_pcode`/`test_vmrun`
non hanno l'opzione; `REPL -D` vale per le direttive della REPL, non per l'esecuzione di un file).
Il modo si prova aggiungendo in testa al file **`#DEF MASUME_MODE_<X>`** (equivalente in-sorgente di
`-D`, come documentato in `TODO.md` §Verification). Il supporto CLI è tracciato nel gap
`Todo/(open) - CLI-Args-e-Env-per-Applicazioni-BASIC` del vault del monorepo.

Un nome di variabile che è una keyword BASIC (`ITEMS`, `INTO`, `AT`, `NAME`, `TEXT`, …) va
rinominato: `items` colliderebbe con la clausola `ITEMS` dei comandi di controllo — vedi
`Hard-Lessons/ITEMS-Keyword-Collision-Variabile` nel vault del monorepo (in `RunTui` l'array si
chiama `menuItems`).

Eseguire da `shared/APPS/Masume/` (o da una sua sottocartella), così il project file `.masume.toml`
viene trovato: l'app cerca nella working directory e nei suoi antenati.

**Nota ambiente:** `GMBasicSharp/Executor` è una **Library** (`OutputType` non `Exe`), quindi
`dotnet run --project Executor` non è un percorso valido — usare i runner qui sopra. Il **PCode**
engine è questo port's verified engine; l'**IL** ha un bug pre-esistente e generale: un array a
dimensione fissa dichiarato a livello modulo/GLOBAL non condivide lo stato tra SUB (confermato con
a minimal repro unrelated to this app — every SUB sees its own stale/default copy) — this port's
`g_prof*`/`g_query*`/`g_param*` global arrays hit it directly, so **IL is not currently usable for
this app**. Tracked as a new AiWiki bug (see `TODO.md`).

## Recorded verification

The "Verification" section of [`TODO.md`](TODO.md) lists, for every verified claim about this
port, the exact command, the backend leg and the observed output.