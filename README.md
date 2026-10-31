# GMBasic (GM-Basic)

> **AI-powered BASIC — three independent backends, SDL2 graphics, six database engines, full TUI, AI agents.**
>
> BASIC with 500+ built-in functions, compiled by three independent backends (IL/NativeAOT, a C
> virtual machine, a C99 transpiler), SDL2 GPU-accelerated graphics (sprites, TTF fonts, joystick),
> six embedded database engines (ISAM B-Tree, DBF/xBase, SPEED LSM, LEDGER, SQLite, VectorDB),
> AI agents with tool-calling, MCP server, RAG pipeline, true-color Clipper-compatible TUI (20+
> controls), HTTP/WebSocket server, linear algebra, statistical analysis, trading/finance, and a
> full networking stack.

**Repository:** [github.com/c-vision/GMBasic](https://github.com/c-vision/GMBasic)

## Repository layout

**One repository, no submodules:** `git clone` is enough, `--recurse-submodules` is not needed.
The folders are domains, not separate repos:

| Folder | Contents |
|---|---|
| `GMBasic/` | **C** compiler and runtime: frontend, C99Native, VM/C, shared runtime, native libraries |
| `GMBasicSharp/` | **C#** compiler and runtime: lexer/parser/linker, IL backend, StdLib, REPL/CLI |
| `shared/` | material shared by both: MiniTests, samples, spec/registry, settings, skills, plugins |
| `GMBasicIDE/`, `GMBasicTuiIde/` | native macOS IDE (VB6-style form designer, code window, debug, DataGrid) and the TUI IDE |
| `tools/` | tooling (BASIC/C/shell) at the repo root, shared by both worlds |

## Download: what, which command, where it lands

Large data is **not in git**. The code lives here; native libraries, market feeds and brains live
in the **releases** of two companion repos and are downloaded **only when needed** (resumable,
**sha256 verified** before extraction).

| repo | visibility | what you find there |
|---|---|---|
| **`c-vision/GMBasic`** | private | this monorepo: compilers, runtime, samples, tools |
| **`c-vision/GMBasicModules`** | public | **binaries and data**: native libraries (one release per group), market feeds (`fixtures-v1`), ML/Chromium/OCR components |
| **`c-vision/GMBasicBrains`** | private | **brains** (Second Brain): RAG and knowledge graph, shipped as release assets |

The other two repos never need to be cloned: the commands below fetch from their releases. From a
checkout use `dotnet run --project GMBasicSharp/REPL -- …`; with the distributed binary it is
simply `gmb …`.

### First time — the four commands that actually matter

```sh
dotnet run --project GMBasicSharp/REPL -- libs ensure            # missing native libraries
dotnet run --project GMBasicSharp/REPL -- fixtures ensure        # market feeds for the samples
dotnet run --project GMBasicSharp/REPL -- brain login --github   # once: token for the private repo
dotnet run --project GMBasicSharp/REPL -- brain install brainvault
```

`ensure` downloads **only what is missing**, so you can re-run it whenever you like.

### What it downloads, with which command, and where it puts it

| item | command | destination |
|---|---|---|
| **native libraries** | `gmb libs ensure` (`status`, `check`, `prune`) — also automatic on first use of a function that needs one | `~/.gmbasic/libs/<rid>/` (in a development checkout the freshly built `GMBasic/build/libs/<rid>/` wins) |
| **external components** (torch/ML, Chromium, Obscura, OCR) | `gmb setup TORCH` · `BROWSER` · `OBSCURA` · `OCR` · `ALL` (or automatic on first use) | `~/.gmbasic/libs/<component>/<rid>/` |
| **market feeds** (`markets`, `option_chains`, `rates`, `darkpool`) | `gmb fixtures list` · `ensure` · `pull markets` | `shared/samples/Fixtures/<name>/` — the **only** place, no symlinks |
| **brains** (RAG and/or graph) | `gmb brain install <name[@ver]>` · `list` · `remove` (from BASIC: `AI BRAIN INSTALL "name"`) | `~/.gmbasic/brains/<name>/<version>/{rag,graphs}` |
| **packages** with external dependencies | `gmb pkg install <name[@ver]>` · `gmb restore` · `gmb update` | `~/.gmbasic/packages` (client) · `<checkout>/packages` (development) · `<exe>/packages` with `--local` · `vendor/` for pins |
| **skill/plugin marketplaces** | `AI SKILLS MARKETPLACE "url"` · `AI PLUGINS MARKETPLACE "url"` | `~/.gmbasic/{skills,plugins}-marketplaces` |

**What is NOT downloaded:** skills, plugins and settings live in the repo (`shared/skills`,
`shared/plugins`, `shared/settings`). When you compile a program, the **bundle** next to the
executable carries only what is needed: minimal settings, `Resources/`, the declared brains
(`<exe>.brains.txt`), the libraries actually used (`<exe>.libs.txt`), the **providers**
(`config/providers.json`, always) and — **only when declared** in the program's `app.settings.json`
— `skills`, `plugins` and extra config files:

```json
{ "brains": ["brainvault"], "fixtures": ["markets"], "skills": ["board"], "plugins": ["web-fetch"], "config": ["mcp.json"] }
```

### Where the state lives (and what never goes into git)

- **`~/.gmbasic/`** — per-user data root: `libs/`, `brains/`, `packages/`, `skills-marketplaces/`,
  `plugins-marketplaces/`, `github.token` (`chmod 600`).
- **`GMBasic/config/`** (or `<exe>/config` in a distributed program) — **local state**: active
  providers, MCP servers, installed brains, skill/plugin indexes. **Never in git**: it holds
  machine-specific paths. If a file is missing, the runtime falls back to the **seed** in
  `shared/settings/` or to the defaults in the code.
- **`shared/samples/Fixtures/`** — the feeds, and nothing else.

## Build & run

```sh
dotnet build GMBasicSharp/GMBasicSharp.slnx
```

There is **no single "run the interpreter"** entry point, because there are three independent
backends. Each has its own:

```sh
# IL backend (pure C#, JIT by default; -n = NativeAOT)
dotnet run --project GMBasicSharp/Executor -- <file.bas>   # compiled IL, JIT (IL only; needs .NET 10)
dotnet run --project GMBasicSharp/REPL -- -n <file.bas>    # -n/--native: NativeAOT executable

# C VM (external process gmb_vm_host)
cmake -S GMBasic/native/gmb_vm_c -B GMBasic/native/gmb_vm_c/build && cmake --build GMBasic/native/gmb_vm_c/build
dotnet run --project tools/runners/GMBASIC.Conformance -- vmtest <file.bas>
# (or set GMBASIC_VM_HOST=GMBasic/native/gmb_vm_c/build/gmb_vm_host, or copy it next to the test executable)

# C99Native backend (C frontend + emitter .c, `cc` against the shared C runtime).
# The script cds into GMBasic/ itself, so sample paths are relative to GMBasic/ (../shared/...)
bash tools/sh/gmb_c99native_run.sh ../shared/samples/Community/Network/WebSocketEcho.bas

# All four legs on the same sample (IL · PCode · VM/C · C99Native), comparing normalized output
zsh tools/sh/parity4 ../shared/samples/Community/Ai/AiPlugins.bas
```

Development mode (skips the license gate): set `GMBASIC_DEV_MODE=1` before any of the commands
above.

## Architecture

Front-end (shared by all backends, all C#):

```
Lexer → Parser → Linker → Compiler (AST/P-Code)
```

From there, compilation forks into **three independent backends**:

| Backend | Language | Output |
|---|---|---|
| **IL Backend** | pure C#, no bridge to C | IL compiled in-process, JIT (framework-dependent) by default; NativeAOT executable with `-n` |
| **VM in C** | pure C | executes the P-Code with a C virtual machine (Clipper-style: the bytecode ships with the executable) |
| **C99Native** | pure C output (a C frontend + C emitter produce it) | direct C99 source, no VM, no bytecode at runtime |

The VM in C and the C99Native backend share the same C runtime library
(`GMBasic/native/gmb_runtime_c/`): no duplicated built-in implementations between them.

## Sample catalog

All samples are runnable BASIC programs. Paths are relative to the repo root; each folder is a
category, and `Community/` is the main one.

### Language and core semantics

| Category | What the samples show |
|---|---|
| [Arrays](shared/samples/Community/Arrays/) | array literals, sorting with comparators, slices and multi-dimensional processing |
| [Bits](shared/samples/Community/Bits/) | bitwise operators and shifts, full coverage |
| [Branches](shared/samples/Community/Branches/) | every `IF` form (block, single-line, nested, `ELSEIF`) |
| [Classes](shared/samples/Community/Classes/) | classes, inheritance, abstract members, array fields, bare method calls |
| [Commands](shared/samples/Community/Commands/) | classic commands: `RUN` chains, line numbers, `DIRECT CAST` |
| [Comprehensive](shared/samples/Community/Comprehensive/) | one big showcase of the feature set (`FeaturesShowcase.bas`) |
| [ControlFlow](shared/samples/Community/ControlFlow/) | `GOSUB`/`RETURN`, `ON ERROR`, interactions between control flow and exceptions |
| [Exceptions](shared/samples/Community/Exceptions/) | `TRY/CATCH`, rethrow, `ON ERROR RESUME` |
| [Functions](shared/samples/Community/Functions/) | `SUB`/`FUNCTION`, parameters, closures, recursion |
| [Lexer](shared/samples/Community/Lexer/) | pre-processing directives, list literals, lexical edge cases |
| [Loops](shared/samples/Community/Loops/) | `FOR`, `DO/LOOP`, `WHILE`, `FOR EACH`, matrix destruction, thread flags |
| [Modules](shared/samples/Community/Modules/) | `IMPORT`/`REQUIRE`, aliases, import priority, structural canaries |
| [Namespaces](shared/samples/Community/Namespaces/) | namespace declaration and qualified access |
| [Operators](shared/samples/Community/Operators/) | short-circuit evaluation, XOR swap, operator precedence |
| [Optimization](shared/samples/Community/Optimization/) | constant folding and optimizer behaviour |
| [Preprocessor](shared/samples/Community/Preprocessor/) | multi-target preprocessing (`#IF`-style directives) |
| [Scopes](shared/samples/Community/Scopes/) | block scopes, `BYREF`, variable lifetime |
| [Stricts](shared/samples/Community/Stricts/) | `STRICT` mode: implicit creation, incompatible comparisons, array initializers |
| [Strings](shared/samples/Community/Strings/) | formatting, interpolation, search and replace |
| [Types](shared/samples/Community/Types/) | `ANY`, UDTs, nullable types, logical/bitwise operators |

### Data, files and storage

| Category | What the samples show |
|---|---|
| [Collections](shared/samples/Community/Collections/) | lists, dictionaries, nested collections |
| [Data](shared/samples/Community/Data/) | `DATA`/`READ`/`RESTORE` |
| [Databases](shared/samples/Community/Databases/) | DBF/xBase, ISAM, joins, indexes, loading documents for RAG |
| [Datetime](shared/samples/Community/Datetime/) | dates, times, arithmetic between dates |
| [Files](shared/samples/Community/Files/) | file and directory I/O, error handling on missing paths |
| [Json](shared/samples/Community/Json/) | parsing, building, iterating JSON |
| [SQL](shared/samples/Community/SQL/) | embedded SQL execution and DataFrame-style results |
| [TimeSeries](shared/samples/Community/TimeSeries/) | rolling windows, resampling, numeric helpers |
| [VectorDB](shared/samples/Community/VectorDB/) | vector collections, similarity search, RAG evaluation |

### AI, agents and knowledge

| Category | What the samples show |
|---|---|
| [Ai](shared/samples/Community/Ai/) | agents, plugins, skills, MCP, permissions, memory, RAG, full AI surface coverage |
| [AiLLM](shared/samples/Community/AiLLM/) | LLM engines: routing (MLX/llama), tool calling, structured output, CUA |
| [Connectors](shared/samples/Community/Connectors/) | the 52-connector marketplace: discovery, auth, policies, auto-registration |
| [Browser](shared/samples/Community/Browser/) | headless Chromium via CDP: navigate, extract, screenshot, human-like input |
| [Torch](shared/samples/Community/Torch/) | tensor runtime: arithmetic, models, MiniLM embeddings |
| [graph](shared/samples/graph/) | knowledge graph: build, query, Louvain/max-flow, brain-backed Q&A |
| [cua](shared/samples/cua/) | computer-use agents and their diagnostics |

### Math, science and finance

| Category | What the samples show |
|---|---|
| [Algebra](shared/samples/Community/Algebra/) | vectors and matrices through the algebra library |
| [Analysis](shared/samples/Community/Analysis/) | numerical analysis: real interpolation, shared-library advanced helpers |
| [Finance](shared/samples/Community/Finance/) | financial functions: loans, APR, TVM, full coverage |
| [Math](shared/samples/Community/Math/) | SciPy-parity numerics: curve fitting, differential evolution, FFT integration |
| [Matrix](shared/samples/Community/Matrix/) | SIMD matrix operations and decompositions |
| [Signal](shared/samples/Community/Signal/) / [Signals](shared/samples/Community/Signals/) | digital filters and signal generation |
| [Spatial](shared/samples/Community/Spatial/) | distances and spatial queries |
| [Trading](shared/samples/Community/Trading/) | backtesting, indicators, extended finance functions |

### Graphics, UI and input

| Category | What the samples show |
|---|---|
| [graphics](shared/samples/Community/graphics/) | SDL2 drawing, controls and widgets, fonts, palettes |
| [Sprites](shared/samples/Community/Sprites/) | sprite loading, movement and collision |
| [Controls](shared/samples/Community/Controls/) | the 20+ TUI/GUI controls (alerts, splitters, grids, forms) |
| [Console](shared/samples/Community/Console/) | colors, cursor control, clipboard, advanced console output |
| [windows](shared/samples/Community/windows/) | window creation, colors, Clipper-style windows |
| [UI](shared/samples/Community/UI/) | UI automation and UI types |
| [Events](shared/samples/Community/Events/) | event handlers, control events, `ON` dispatch |
| [Mouse](shared/samples/Community/Mouse/) | mouse state, graphic painting, input tests |
| [GameDev](shared/samples/Community/GameDev/) | joystick, Pac-Man style game loop |
| [Audio](shared/samples/Community/Audio/) | audio playback and audio controls |
| [Computer](shared/samples/Community/Computer/) | screen recording and computer-control primitives |
| [Tray](shared/samples/Community/Tray/) | system tray integration |

### Network, web and distributed

| Category | What the samples show |
|---|---|
| [Network](shared/samples/Community/Network/) | TCP/UDP, TLS, WebSocket, HTTP handlers on the GPM |
| [Http](shared/samples/Community/Http/) | HTTP client, JSON bodies, configuration, auth demos |
| [Web](shared/samples/Community/Web/) | registry server, HTML-to-Markdown conversion |
| [Concurrency](shared/samples/Community/Concurrency/) | green threads: channels, atomics, async helpers, cancellation |
| [Messaging](shared/samples/Community/Messaging/) | pub/sub and delivery monitoring |
| [Team](shared/samples/Community/Team/) | multi-agent teams: broadcast, extensions, strategies |
| [Workflow](shared/samples/Community/Workflow/) | DAG workflows, steps, retries |
| [Security](shared/samples/Community/Security/) | authentication flows, MVC auth, encodings |
| [Crypto](shared/samples/Community/Crypto/) | symmetric/asymmetric crypto, hashing, a blockchain sample |

### Text, tooling and interop

| Category | What the samples show |
|---|---|
| [Clipper](shared/samples/Community/Clipper/) | Clipper/xBase compatibility: code blocks, arrays, windows, database commands |
| [compression](shared/samples/Community/compression/) | gzip/zstd/brotli-style compression APIs |
| [Documents](shared/samples/Community/Documents/) | `CONVERT` and document conversion (PDF, Markdown, Office) |
| [FFI](shared/samples/Community/FFI/) | calling C/C++/Go libraries, drop-in library loading |
| [Grep](shared/samples/Community/Grep/) | file search engine, output formats |
| [PackageRegistry](shared/samples/Community/PackageRegistry/) | a package registry server in BASIC |
| [Process](shared/samples/Community/Process/) | spawning and driving external processes |
| [System](shared/samples/Community/System/) | settings/paths, command-line arguments, system info |
| [C99](shared/samples/Community/C99/) | modules and library calls specific to the C99Native backend |

### Other sample roots

| Folder | Contents |
|---|---|
| [Commercial](shared/samples/Commercial/) | samples that require the commercial edition (AI, databases, Clipper, parser stress) |
| [Native](shared/samples/Native/) | native interop entry points (`HelloNative.bas`) |
| [packages](shared/samples/packages/) | samples materialized by `gmb pkg install` for installed packages |
| [Fixtures](shared/samples/Fixtures/) | input data (not programs): small fixtures in git, the four market feeds downloaded on demand |

## AI & Agents

| Subsystem | Description | Location |
|---|---|---|
| **Agents** | agent memory store (ISAM-backed), tool loop, agent server | `GMBasicSharp/common/AI/Agents/` |
| **MCP** | Model Context Protocol server (stdio/http), audit sink | `GMBasicSharp/common/AI/Mcp/` |
| **Providers** | LLM provider abstraction (OpenAI-compatible, Ollama, ONNX) with auto-failover | `GMBasicSharp/common/AI/Providers/` |
| **RAG** | retrieval-augmented generation pipeline, vector runtime | `GMBasicSharp/common/AI/` |
| **Graph** | knowledge graph store, PageRank/Louvain, provenance | `GMBasicSharp/common/AI/Graph/` |
| **Brains** | installable RAG + graph bundles | `~/.gmbasic/brains` (see [Download](#download-what-which-command-where-it-lands)) |
| **Skills / Plugins** | prompt archives and agent capability descriptors | `shared/skills/`, `shared/plugins/` |

## Graphics

SDL2-based GPU-accelerated console. Full-screen, multiple screens, sprites, fonts, TTF rendering.

```basic
SCREEN 640, 480, 32
CIRCLE (320, 240), 100, RGB(255, 0, 0)
SPRITE 1, "sprite.png", 0, 0
```

Dependencies: `libSDL2.dylib`, `libSDL2_ttf.dylib`, `libfreetype.6.dylib`, `libharfbuzz.0.dylib`
(downloaded into `~/.gmbasic/libs/<rid>/` on demand).

## Standard Library

| Namespace | Functions |
|-----------|-----------|
| `SYSTEM` | `Next`, `Tab`, `Sys`, `Iif`, `Empty`, `SizeOf`, `VarPtr`, `StrPtr`, `UBound`, `LBound`, `Count`, `TypeName`, `CType`, `Choose` |
| `MATH` | `Rnd`, `Pi`, `Abs`, `Sgn`, `Sqr`, `Log`, `Exp`, `Sin`, `Cos`, `Tan`, `Atn`, `Round`, `Floor`, `Ceiling`, `Min`, `Max`, `Clamp`, `Fact`, `Perm`, `Comb` |
| `STRINGS` | `Left`, `Right`, `Mid`, `Len`, `Asc`, `Chr`, `Str`, `Instr`, `Replace`, `Split`, `Join`, `Format`, `Upper`, `Lower`, `Trim`, `Reverse`, `Pascal`, `Camel`, `Snake`, `Kebab`, `Contains`, `StartsWith`, `EndsWith` |
| `DATETIME` | `Now`, `Today`, `DateAdd`, `DateDiff`, `DateSerial`, `TimeSerial`, `WeekDay`, `EoMonth`, `DateFromUnix`, `UnixTimestamp` |
| `CONVERSIONS` | `CStr`, `Val`, `Hex`, `Oct`, `Bin`, `CSng`, `CDbl`, `CInt`, `CLng`, `CByte` |
| `IO` | `EOF`, `LOF`, `FreeFile`, `Open`, `Close`, `Read`, `Write`, `Seek`, `FileExists`, `DirExists`, `MkDir`, `RmDir`, `ChDir`, `Cwd` |
| `CONCURRENCY` | `SleepAsync`, `TaskYield` |
| `JOYSTICK` | `Joy`, `JoyCount`, `JoyName`, `JoyPresent` |

## Databases

Six embedded database engines, all reachable from BASIC. C# implementation:
`GMBasicSharp/common/Databases/` (plus `GMBasicSharp/modules/Database/`):

| Engine | Type |
|--------|------|
| **ISAM** | key-value B-tree, WAL, concurrent readers |
| **DBF** | dBASE file format (read/write, NTX/NDX indexes, compound, memo) |
| **SPEED** | LSM-tree, high throughput, crash recovery |
| **LEDGER** | append-only ledger with PACK compaction |
| **SQLite** | embedded SQL via Microsoft.Data.Sqlite |
| **VectorDB** | vector database with cosine similarity, IVF index |

A parallel **pure-C implementation** of ISAM/DBF/SPEED/LEDGER lives in
`GMBasic/native/gmb_db_c/` for the VM/C and C99Native backends — advanced but **not linked by
default** (`GMB_HAS_COMMERCIAL_DB` CMake option, off by default). It is tested against the C#
implementation as oracle in `GMBasicSharp/tests/GMBASIC.Database.Native.Tests/`.

## HTTP & Networking

```basic
' HTTP client
DIM response = HTTP.Fetch("https://api.example.com/data")

' WebSocket client
DIM ws = WS.Connect("wss://echo.websocket.org")
WS.Send(ws, "Hello")
DIM reply = WS.Receive(ws)

' TCP/UDP
DIM sock = TCP.Connect("example.com", 80)
TCP.Send(sock, "GET / HTTP/1.1\r\n\r\n")
DIM data = TCP.Receive(sock, 1024)

' HTTP server
HTTP.Listen 8080
```

## Concurrency (GPM)

Cooperative multitasking with green threads (goroutine-style `GO SUB`/channels), modeled on Go's
GPM (M:P:G). **Each backend has its own GPM implementation** — VM/C uses a pthread-based model,
C99Native a green-thread model. Language surface (identical across backends):

```basic
' Start a concurrent task
GO SUB BackgroundTask()

' Synchronization
WAIT      ' Wait for all GO tasks
SEND ch, value    ' Channel send
RECEIVE ch => var ' Channel receive

' Sleep without blocking the scheduler
CALL SleepAsync(1000)
```

## Language features

| Feature | Status |
|---------|--------|
| Line-numbered BASIC (classic) | ✅ |
| Named subroutines (`SUB`/`FUNCTION`/`END SUB`) | ✅ |
| Block IF/THEN/ELSE/END IF | ✅ |
| SELECT CASE | ✅ |
| FOR/NEXT (single and nested) | ✅ |
| DO/LOOP WHILE/UNTIL | ✅ |
| WHILE/WEND | ✅ |
| EXIT FOR/DO/WHILE | ✅ |
| CONTINUE FOR/DO/WHILE | ✅ |
| Labeled loops (`EXIT OuterLoop`, `CONTINUE Middle`) | 🔶 |
| FOR EACH (on collections) | 🔶 |
| GOTO/GOSUB/RETURN | ✅ |
| ON ERROR GOTO/RESUME | ✅ |
| TRY/CATCH | ✅ |
| STATIC variables | ✅ |
| User-defined types (UDT) | ✅ |
| Classes with methods/properties | ✅ |
| Generics (`LIST OF T`, `FUNCTION Foo<T>`) | ✅ |
| Nullable types (`INTEGER?`, `STRING?`) | ✅ |
| Attributes (`<Serializable>`, `<Required>`) | ✅ |
| Lambda expressions, closures | ✅ |
| List/dict comprehensions | ✅ |
| Macro system | ✅ |
| Module system (`IMPORT`/`REQUIRE`) | ✅ |
| `FOR EACH` (on arrays, lists) | 🔶 |

✅ = working, 🔶 = partial/known issue

## Tests

| Area | Command |
|------|---------|
| Compile + P-Code codegen, all samples | `dotnet run --project tools/runners/GMBASIC.Conformance` |
| VM/C (compile → P-Code → run as external `gmb_vm_host`) | `dotnet run --project tools/runners/GMBASIC.Conformance -- vmtest <file.bas>` |
| IL backend execution (xUnit, incl. Clipper/Loops samples) | `dotnet test GMBasicSharp/tests/GMBASIC.IlBackend.Tests` |
| C99Native backend (C frontend + emitter → `cc` → run) | `bash tools/sh/gmb_c99native_run.sh ../shared/samples/<file>.bas` |
| All four legs on one MiniTest | `zsh tools/sh/test_minitests_parity.sh "<domain>/<file>.bas"` |
| Native C database module (oracle vs. C# implementation) | `dotnet test GMBasicSharp/tests/GMBASIC.Database.Native.Tests` |
| Native C runtime/VM unit tests | CMake build of `GMBasic/native/gmb_runtime_c` (`--target test`) |
| AI agents/RAG | `dotnet test GMBasicSharp/tests/AI.Tests` |
| AI eval harness (fixture, no network) | `dotnet run --project tools/runners/AI.Eval` |
| ABI contract C→C# (13 dylibs) | `bash shared/runtime-abi/check_all.sh` |

## Commands reference

See [COMMANDS.txt](./COMMANDS.txt) for the complete list of supported commands and functions.

## REPL commands

`RUN`, `LIST`, `NEW`, `SAVE`, `LOAD`, `DELETE`, `SAMPLE <n>`, `AUTO`, `RENUM`, `CLS`, `HELP`, `QUIT`,
`LEXER ON/OFF`, `PARSER ON/OFF`, `EXECUTOR ON/OFF`.

## Configuration

- `shared/settings/system.settings.json` — global defaults (paths, console, engine, compiler)
- `<project>/app.settings.json` — per-project overrides (`paths`, `consoleConfiguration`, `engine`,
  plus the optional `brains`/`fixtures`/`skills`/`plugins`/`config` declarations)
- `GMBasic/config/` (or `<exe>/config`) — **local state**, never in git: see
  [GMBasic/config/README.md](GMBasic/config/README.md)

## License

Proprietary. A license key is required for full execution (runtime); compilation and bytecode
disassembly work without one.

Development mode: `GMBASIC_DEV_MODE=1` bypasses the licensing gate.
