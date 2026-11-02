# Migrazione alla REPL unica — nessuna funzione persa in silenzio

Questo file è la **matrice di tracciamento** tra la vecchia REPL C# (`GMBasicSharp/REPL`,
eliminata in F6b) e il mondo attuale. Regola: ogni funzione ha **una casa** oppure un
**messaggio esplicito** con l'alternativa. Niente sparisce senza dirlo.

Programmi attuali:

| programma | ruolo |
|---|---|
| `REPL/build/gmb_repl` | REPL canonica (C) — comandi, IDE, console, package/fixtures |
| `GMBasicSharp/CompilerCli` → `gmbcs` | engine C# headless: `check`, `dump ast\|pcode\|il\|disasm`, `prginfo`, `build`, `run`, `eval`, `debug`, `aibench` |
| `GMBasicSharp/Cli` → `gmb` | CLI amministrativo C#: pacchetti, brain, libs, setup, daemon, rag/graph, grep, convert/convfmt, hf/mlx |
| `Notebook/build/gmb_notebook` | notebook server (C): sessione BASIC via HTTP su 127.0.0.1 |

## 1. Comandi della REPL (70 nella mappa C#)

Tutti presenti nella REPL C con lo stesso nome; `AIBENCH` (unico assente dopo F6b) è stato
riportato e usa `gmbcs aibench`. `NOTEBOOK` non era **mai registrato** nella mappa C# (codice
morto): la funzione è ripristinata come progetto autonomo `Notebook/`.

Comandi riportati nel mondo C o instradati (vedi commit F3/F6b):

| comando | dove |
|---|---|
| `FIXTURES` | runtime C `gmb_fixtures_*` |
| `SETUP` | runtime C (cache libs) |
| `LLM` | runtime C (AIPATH) |
| `ADD`/`REMOVE`/`BUMP`/`RESTORE`/`UPDATE`/`ROLLBACK`/`TRUST`/`CACHE` | runtime C `gmb_pkg_*` |
| `AIBENCH` | `gmbcs aibench` |
| `HF`/`MLX` | `gmb hf` / `gmb mlx` (CLI C#) |
| `RAG` | `gmb rag …` |
| `GRAPH` | BUILD/STATUS in C (`rpl_graph_build`); altre sottoforme a `gmb graph …` |
| `BRAIN INSTALL/EXPORT/SIGN` | `gmb brain …` (USE/DETACH/LIST/INFO in C) |
| `LIBS UPDATE/CHECK/PUBLISH` | `gmb libs …` |
| `CONVERT` / `CONVFMT` | `gmb convert` / `gmb convfmt` |
| `BSP <dir>` | **C**: `gmb_bsp_compile` + `gmb dump chunk` → `<pagina>.bsp.chunk` |
| `IL` / `PCODE` | `gmbcs dump il|pcode` (solo se engine C# presente) |
| `EDIT`/`IDE` | IDE integrato C (`REPL/ide.c`) |
| `EVAL` | `gmb eval` (C) / `gmbcs eval` (engine C#) |

## 2. Flag di avvio

| flag (C#) | stato attuale |
|---|---|
| `-noconsole`, `-c`/`-console`, `--sdl`, `-s`, `-c64` | `gmb_repl` (console System/Standard/SDL/C64) |
| `-size W,H`, `-font nome,size`, `-w`, `-h` | `gmb_repl` |
| `--pcode` | `gmb_repl` (C: vmc default; engine C#: P-Code) |
| `--ast` | **implementato**: `gmb_repl <file> --ast` (e `gmb <file> --ast` via `gmbcs dump ast`) |
| `-repl` | **implementato**: dopo il file resta nella REPL |
| `--dump-il` | **implementato** con `--engine=csharp` (esegue e produce il dump IL) |
| `-bsp <dir>` | **implementato** in C (`rpl_bsp_compile_dir`) |
| `-D name=val` | REPL → `gmb`/`gmbcs`; CLI → `gmbcs` |
| `--il-module <dll>` | solo con `--engine=csharp` (inoltrato a `gmbcs`); senza engine: errore esplicito |
| `-e`/`-executor` | `gmb_repl -e` (log settings) |
| `-gpm[:mode]` / `GPM <mode>` | **implementato**: `auto|always|linear` passa a `gmb --gpm` (emissione C99). `auto` = rilevamento (default effettivo), `always` = forza lo scheduler (`gmb_gpm_start` sempre), `linear` = percorso diretto se lineare (mai crash) |
| `--run <cmd>` | `gmb_repl --run <cmd>` |
| `-m`/`--managed` | **non nel REPL**: `gmbcs build --target il` (messaggio esplicito se passato) |
| `-n`/`--native` | **non portato** (AOT): percorso nativo canonico `gmb build` — messaggio esplicito |
| `-full` | **non portato** (closure managed completa): `gmbcs build --target il` — messaggio esplicito |
| `--debug-tcp <port>` | **non implementato nel mondo C** (debug TCP per IDE esterni): avviso esplicito; usare `IDE` o `gmbcs debug` |
| `--target wasm`, `-o` | **rimossi** con la REPL C# (POC): messaggio esplicito; per il C usare `gmb dump c` |
| `--no-optimize` | C: nessun ottimizzatore (avviso esplicito); engine C#: non esposto |

## 3. CLI `gmb` (comandi amministrativi)

Tutti conservati in `GMBasicSharp/Cli`: `init`, `pack`, `keygen`, `bump`, `add`, `remove`,
`login`, `register`, `logout`, `cache`, `admin`, `pkg`, `brain`, `fixtures`, `publish`,
`restore`, `update`, `trust`, `rollback`, `libs`, `setup`, `daemon`, `rag`, `graph`, `grep`.
Recuperati dalla REPL eliminata: `convert`, `convfmt`, `hf`, `mlx`.

I flag che appartengono alla REPL (`-size`, `-font`, `-c`, `-s`, `-c64`, `-noconsole`,
`--sdl`, `-gpm`, `--debug-tcp`, `--run`, `-bsp`) o alla POC wasm (`--target`, `-o`) **non**
vengono ignorati: il CLI stampa un errore esplicito con il programma giusto da usare.

## 4. Esecuzione dei programmi

| modalità | comando |
|---|---|
| engine C# IL (managed) | `gmbcs run <file> --engine il` (o `gmb <file>`) |
| engine C# P-Code | `gmbcs run <file> --engine pcode` (o `gmb <file> -pcode`) |
| debug engine C# | `gmbcs debug <file> --break N …` |
| mondo C (VMC/C99) | `gmb_repl <file>` / `gmb run --project <dir>` |

## 5. Aperti (tracciati, mai silenziosi)

1. **NativeAOT `-n`** (build nativo dal mondo C#): non portato in `gmbcs`; il nativo canonico
   è il compilatore C (`gmb build`).
2. **`-full`** (closure managed completa copiata): non portato.
3. **`--debug-tcp`** (debug server TCP per IDE esterni): non implementato nel mondo C.
4. ~~GPM mode sul runner C99~~ — **fatto**: `gmb --gpm auto|always|linear` arriva all'emettitore
   C99 (`gmb_c99_set_gpm_mode`); REPL `-gpm`/`GPM` lo propagano a `RUN`/`COMPILE`.
5. ~~BSP index.bsp~~ — **risolto** (2026-11-02): la causa non era il front-end:
   - il `.bas` generato veniva scritto in `/tmp`, e la discovery del compilatore (ricorsiva
     sulla cartella dell'entry) includeva `.inc` estranei lasciati lì (duplicati `EscapeHtml`).
     Ora `BSP` usa una **cartella temporanea privata** (`mkdtemp`);
   - `REPLACE$` mancava in **VM/C** (A1): aggiunta (via `gmb_str_replace`, MiniTest
     `strings/replace_dollar.bas` verde su 4 backend);
   - il compilatore A1 non mappava i nomi `AI.AGENTLIST/AGENTEVENTS/AGENTGET/MEMORYASK/
     WORKFLOW*` (implementati nel VM): aggiunti. `BSP` su `shared/webapp/pages` → **2 OK, 0 FAIL**.
   Resta una divergenza preesistente (non BSP): `REPLACE$("abc","","-")` — IL/PCode lanciano,
   C99/C99Native inseriscono.
