# GMBasic (GM-Basic)

> **AI-Powered BASIC — Three Compiler Backends, SDL2 Graphics, 6 Database Engines, Full TUI, AI Agents.**
>
> BASIC with 500+ built-in functions, compiled to three independent backends (IL/NativeAOT, a C
> virtual machine, a C99 transpiler), SDL2 GPU-accelerated graphics (sprites, TTF fonts, joystick),
> six embedded database engines (ISAM B-Tree, DBF/xBase, SPEED LSM, LEDGER, SQLite, VectorDB),
> AI agents with tool-calling, MCP server, RAG pipeline, true-color Clipper-compatible TUI (20+ controls),
> HTTP/WebSocket server, linear algebra, statistical analysis, trading/finance, and full networking stack.

GMBASIC — AI-powered BASIC compiler with **three independent backends** (IL/.NET NativeAOT, a C
virtual machine, and a C99 transpiler), SDL2 graphics, embedded databases, AI agents, MCP server,
and networking. See the Architecture section below for the full picture.

**Repository:** [github.com/c-vision/GMBasic](https://github.com/c-vision/GMBasic)

## Repository layout

Quattro submodule, ognuno con la propria storia:

| Submodule | Contenuto |
|---|---|
| `GMBasic/` | compilatore e runtime **C**: frontend, C99Native, VM/C, runtime, librerie native |
| `GMBasicSharp/` | compilatore e runtime **C#**: lexer/parser/linker, backend IL, StdLib, REPL |
| `shared/` | materiale comune ai due: MiniTest, sample, spec/registry, settings, docs |
| `GMBasicIDE/` | IDE nativo macOS in stile VB6 (form designer, code window, debug, DataGrid) che usa il compilatore |

## Build & Run

```sh
dotnet build GMBASIC.sln
```

This is a monorepo unifying the deprecated GM-Basic and GMBasicAI repos: no single "run the
interpreter" entry point exists, because there are three independent backends. Each has its own
entry point:

```sh
# IL Backend (pure C#, framework-dependent JIT by default; -n = NativeAOT)
dotnet run --project Executor -- <file.bas>            # IL compilato, JIT (Executor managed, solo IL; richiede .NET 10)
dotnet run --project REPL -- -n <file.bas>             # -n/--native: eseguibile NativeAOT (publish AOT, host REPL)

# VM in C (external process gmb_vm_host, for quick verification)
# richiede il binario Community: cmake -S native/gmb_vm_c -B native/gmb_vm_c/build && cmake --build native/gmb_vm_c/build
# (poi GMBASIC_VM_HOST=native/gmb_vm_c/build/gmb_vm_host, o copialo accanto all'eseguibile del test)
dotnet run --project tests/GMBASIC.Conformance -- vmtest <file.bas>

# C99Native backend (C frontend + emitter generate .c, `cc` against the shared C runtime, run it).
# The old C# C99 transpiler gate (tests/GMBASIC.Backend.C99.Tests) is retired.
# CWD must be GMBasic/ (the script resolves the repo from its own path); sample paths are
# relative to it, hence ../shared/...
(cd GMBasic && bash tools/gmb_c99native_run.sh ../shared/samples/Community/Network/WebSocketEcho.bas)
```

Development mode (bypasses the licensing gate) — set `GMBASIC_DEV_MODE=1` before any of the
commands above.

## Architecture

Front-end (shared by all three backends, all C#):

```
Lexer → Parser → Linker → Compiler (AST/P-Code)
```

From there, compilation forks into **three independent backends** — no C# PCodeVM (unlike the
deprecated GM-Basic repo, this repo never had one):

| Backend | Language | Output |
|---|---|---|
| **IL Backend** | Pure C#, no bridge to C | IL compilato in-process (JIT, framework-dependent) di default / `-m`; eseguibile NativeAOT via `-n` (IL emission) |
| **VM in C** | Pure C | Executes the P-Code via a C virtual machine (Clipper-style: bytecode ships with the executable) |
| **C99 transpiler** | Pure C output (C# does the generating) | Direct C99 source, no VM, no bytecode at runtime |

The VM in C and the C99 transpiler share the same C runtime library (`native/gmb_runtime_c/`) — no
duplicated built-in implementations between the two.

## AI & Agents

| Subsystem | Description | Location |
|-----------|-------------|----------|
| **Agents** | Agent memory store (ISAM-backed), tool loop, agent server | `common/AI/Agents/` |
| **MCP** | Model Context Protocol server (stdio/http), audit sink | `common/AI/Mcp/` |
| **Providers** | LLM provider abstraction (OpenAI-compatible, Ollama, ONNX) with auto-failover | `common/AI/Providers/` |
| **RAG** | Retrieval-Augmented Generation pipeline, vector runtime | `common/AI/Rag/` |
| **VectorDB** | Pure .NET vector database (embedded) | `common/Databases/VectorDB/` |
| **Model descriptors** | `.gmbarch` files owned by LLMEnv (`ABI/`); folder path in `paths.abiDir`, located on first use ([docs/llmenv-abi.md](docs/llmenv-abi.md)) | `GMBasic/native/gmb_runtime_c/src/gmb_abi.c` |

Note: the AI subsystem is C# only — it currently runs on the IL Backend path. Availability on the
VM/C and C99 backends is not yet verified.

Built-in BASIC keywords: `AI.ASK`, `AI.MCP`, `AI.AGENT`, vector operations.

## Graphics

SDL2-based GPU accelerated console. Full-screen, multiple screens, sprites, fonts, TTF rendering.

```basic
SCREEN 640, 480, 32
CIRCLE (320, 240), 100, RGB(255, 0, 0)
SPRITE 1, "sprite.png", 0, 0
```

Dependencies: `libSDL2.dylib`, `libSDL2_ttf.dylib`, `libfreetype.6.dylib`, `libharfbuzz.0.dylib` (auto-copied on macOS build).

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

Six embedded database engines, all accessible from BASIC. C# implementation:
`common/Databases/` (+ `modules/Database/`, driver commerciali, Community edition esclusa):

| Engine | Type |
|--------|------|
| **ISAM** | Key-value B-tree, WAL, concurrent readers |
| **DBF** | dBASE file format (read/write, indexes NTX/NDX, compound, Memo) |
| **SPEED** | LSM-tree, high-throughput, crash recovery |
| **LEDGER** | Append-only ledger with PACK compaction |
| **SQLite** | Embedded SQL via Microsoft.Data.Sqlite |
| **VectorDB** | Vector database with cosine similarity, IVF index |

A parallel **pure-C implementation** of ISAM/DBF/SPEED/LEDGER exists at `native/gmb_db_c/` for the
VM/C and C99 backends — advanced but **not yet linked by default**
(`GMB_HAS_COMMERCIAL_DB` CMake option, off by default). Tested against the C# implementation as
oracle in `tests/GMBASIC.Database.Native.Tests`.

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
C99 a green-thread model; see `docs/GPM-Comparazione-Implementazioni.md` for the detailed
comparison and history of abandoned approaches. Language surface (same across backends):

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

## Language Features

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
| Compile + P-Code codegen, all samples | `dotnet run --project tests/GMBASIC.Conformance` |
| VM/C (compile → P-Code → run as external process `gmb_vm_host`) | `dotnet run --project tests/GMBASIC.Conformance -- vmtest <file.bas>` |
| IL Backend execution (xunit, incl. Clipper/Loops samples) | `dotnet test tests/GMBASIC.IlBackend.Tests` |
| Clipper-specific IL execution | `dotnet test tests/GMBASIC.Clipper.Tests` |
| C99Native backend (C frontend + emitter → `cc` → run) | `(cd GMBasic && bash tools/gmb_c99native_run.sh ../shared/samples/<file>.bas)` |
| Native C database module (oracle vs. C# implementation) | `dotnet test tests/GMBASIC.Database.Native.Tests` |
| Native C runtime/VM unit tests | `tests/native_tests/run_tests.c` (compilato/eseguito via il build CMake di `native/gmb_runtime_c`) |
| AI agents/RAG | `dotnet test tests/AI.Tests` |
| AI eval harness (fixture, no ollama) | `dotnet run --project tests/AI.Eval` |
| AI eval harness (OpenRouter) | `OPENROUTER_API_KEY=... dotnet run --project tests/AI.Eval -- --openrouter <model>` |

No single test here runs the whole suite with a full golden-file diff across all three backends —
each backend has its own harness, as listed above.

## Commands Reference

See [COMMANDS.txt](./COMMANDS.txt) for the complete list of supported commands and functions.

## REPL Commands

`RUN`, `LIST`, `NEW`, `SAVE`, `LOAD`, `DELETE`, `SAMPLE <n>`, `AUTO`, `RENUM`, `CLS`, `HELP`, `QUIT`, `LEXER ON/OFF`, `PARSER ON/OFF`, `EXECUTOR ON/OFF`.

## Configuration

- `system.settings.json` — default namespaces/consoles
- `app.settings.json` — per-project overrides

## License

Proprietary. Requires a license key for full execution (runtime). Compilation and bytecode disassembly work without a license.

Development mode: `GMBASIC_DEV_MODE=1` bypasses the licensing gate.
