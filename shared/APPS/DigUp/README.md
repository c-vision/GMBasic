# DigUp (GMBasic port) — semantic search in files

**DigUp** finds the files of the computer **from what they say or show**, not from the name: you
describe what you are looking for and the app ranks the closest passages. This is the **GMBasic
port** of its **headless skeleton**, written in BASIC and compilable on the monorepo backends.

## Upstream (source of the port)

| Item | Reference |
|---|---|
| Repository | <https://github.com/ARahim3/DigUp> |
| Licence | **MIT** (attribution required: see below) |
| Model of the original | **EmbeddingGemma 2** (Google DeepMind, 768 dim, multimodal) via llama.cpp |
| Architecture | folders → reader → embedding → **one index** (vectors + text) → query → ranking |

Attribution (MIT): DigUp © ARahim3 (Abdur Rahim), <https://github.com/ARahim3/DigUp>. Original
code MIT; this port takes over **the architecture and the observable behaviour**, not the Swift/Tauri
code.

## What this port does (M1 — headless core)

```
configured folders ─► reading of the text files ─► chunk ─► embedding ─► SQLite
                                                                       │  (text + vector)
query in words ─────► embedding of the query ─► cosine + exact-words boost ─► top-k
```

- **Reader**: text/document/code files by extension (`[index] extensions`).
- **Chunking**: windows of `[index] chunk` characters.
- **Embedding**: `AI.EMBED` of the `%EMBEDDING` capability, with the **model configured in the
  app's settings** (`.digup.toml`, `[llm]`) — never hardcoded (see ADR in the vault).
- **Index**: one SQLite file (`bin/digup.sqlite`) with `path`, `seq`, `text`, `vec` (CSV).
- **Ranking**: delegated to `AI.SEARCH(..., hybrid=1)` — embedding similarity **+** exact words
  (the boost on invoice numbers / error codes is done by the engine, not by the app); the `sourceId`
  (`path#seq`) links every hit to the file and to the passage, which the app resolves in the `chunks` table.

### Modes

| Mode | How it is activated | What it does |
|---|---|---|
| `status` (default) | — | banner, config, presence of the index, number of chunks |
| `index` | `#DEF DIGUP_MODE_INDEX` at the top of the `.bas` | indexes the `roots` (recursive) |
| `search` | `#DEF DIGUP_MODE_SEARCH` | searches `[search] query` and prints the top-k |
| `check` | `#DEF DIGUP_MODE_CHECK` | active embedding provider, dims, index |

> BASIC has no argv: the mode is chosen with `#IFDEF` (the `tools/runners/` runners do not pass
> `-D`) or with `[settings] mode` in `.digup.toml`.

## Configuration (`.digup.toml`, next to the app or in a parent folder)

```toml
[settings]
mode = "status"

[index]
roots = "fixtures"                     # folders to index (relative to the app, csv)
db = "bin/digup.sqlite"                # index (output, gitignored)
extensions = ".txt,.md,.csv,.bas,.json,.toml"
chunk = 300
reindex = "true"

[search]
top = 5
query = "zebra in the savanna"

[llm]
model = "fixture-embedding"            # %EMBEDDING alias/provider (deterministic default)
capability = "embedding"
dir = ""                               # if set: AI REGISTER + activation (local model)
dims = 8                               # fixture=8; real models 384/768/1024
```

- `[llm] dir` set → the app registers the local model (`AI REGISTER "digup-embed",
  %EMBEDDING, dir`) and activates it: a model supported by LLMEnv is used, **not** necessarily the
  original one (EmbeddingGemma 2 is not required).
- `[llm] model` empty → the provider already active stays.
- Unregistered alias → **explicit error**, no silent fallback.

## How to run

From the **monorepo root** (`GMBasicMonoRepo/`):

```sh
# status (default)
tools/sh/harness.sh pcode shared/APPS/DigUp/src/DigUp.bas
tools/sh/harness.sh il    shared/APPS/DigUp/src/DigUp.bas

# index / search: add at the top of the .bas   #DEF DIGUP_MODE_INDEX  (or _SEARCH)
tools/sh/harness.sh pcode shared/APPS/DigUp/src/DigUp.bas

# native executable (release)
REPL/build/gmb_repl shared/APPS/DigUp/src/DigUp.bas

# C99Native (C frontend + emitter → cc → execution; path relative to GMBasic/)
bash tools/sh/gmb_c99native_run.sh ../shared/APPS/DigUp/src/DigUp.bas
```

The index and every output stay in **`bin/`** (compiler default, gitignored): the app **does not**
use `work/` (reserved for samples, see `AGENTS.md` §11.1).

## Fixtures (versioned inputs)

`fixtures/` contains three short texts used by the demo and by the verification: `zebra.txt` (a herd of
zebras), `lease.md` (contract: animals clause), `podcast.txt` (an episode about sleep). They serve to
show the ranking without depending on the user's models or files.

## Recorded verification (2026-11-01, 4 legs + embedding fixture)

> [!note] **State per leg**: `status`, `index` (3 files / 3 chunks) and `search` (top-1
> `fixtures/zebra.txt` 0.333) verified on **IL, PCode, VM/C and C99Native**. Two limits of the IL
> backend are worked around in the code and tracked in the vault: reading lines with `IO.READFILE`+`STRINGS.SPLIT`
> (instead of `IO.READLINES`+`UBOUND`) and the condition `IF isDir <> 0` (instead of `IF isDir`), plus the
> DB path passed as a parameter to `OpenIndex`. Detail:
> `Bugs/(open) - App-DigUp-Trappole-Tipizzazione-JSON-Array` and
> `Bugs/(open) - IL-IF-su-Variabile-BIT-e-SQL-OPEN-con-Globale`.

The search uses **AI.SEARCH** (hybrid: embedding + exact words) on the AI RAG collection; the
scores are those of the engine (with the fixture provider: 0.33 / 0.75 / 1.0 in the three probes).


| step | outcome |
|---|---|
| `status` | config read from `.digup.toml`, index absent → `chunks : 0` |
| `index` | `files indexed : 3`, `chunks : 3` |
| `search` «zebra in the savanna» | 1st `fixtures/zebra.txt#0` (0.333) |
| `search` «pets clause lease» | 1st `fixtures/lease.md#0` (0.75) |
| `search` «REM sleep dreams» | 1st `fixtures/podcast.txt#0` (1.0) |

> With the **fixture** provider the vector is deterministic but **not semantic**: the ranking holds
> thanks to the boost on exact words. For "by meaning" search a real embedding model is needed in
> `[llm] dir` (supported by LLMEnv) — see `TODO.md` §M2.

## Implementation notes (for whoever picks this up)

- **`STRINGS.SPLIT` is not used**: with `IMPORT AI` it returns elements that are **arrays**
  (`System.String[]`) instead of strings → the parsing of CSVs/extensions is done by hand
  (`SplitParts`, `MID$`/`TRIM`) on a global buffer.
- `IO.DIR` returns a **LIST** (`FOR EACH` pattern); the recursion uses `WalkDir`.
- The text of a chunk is saved as a string with doubled quotes (`SqlQuote`); no
  multiple `?` parameters (not used elsewhere in the repo).
- With the fixture embedding the dimension is 8 (`[llm] dims`); with a real model it must be aligned.

## Licence of this port

Code of the port: licence of the GMBasic project. **Mandatory attribution** to the original DigUp
(MIT) as above. The embedding model used depends on the user's configuration and keeps
its own licence (LLMEnv/LLMEngine for the inference).
