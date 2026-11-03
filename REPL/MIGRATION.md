# Migrazione alla REPL unica — nessuna funzione persa in silenzio

Questo file è la **matrice di tracciamento** tra la vecchia REPL C# (`GMBasicSharp/REPL`,
eliminata in F6b) e il mondo attuale. Regola: ogni funzione ha **una casa** oppure un
**messaggio esplicito** con l'alternativa. Niente sparisce senza dirlo.

Programmi attuali:

| programma | ruolo |
|---|---|
| `REPL/build/gmb_repl` | REPL canonica (C) — comandi, IDE, console, package/fixtures |
| `GMBasicSharp/CompilerCli` → `gmbcs` | engine C# headless: `check`, `dump ast\|pcode\|il\|disasm`, `prginfo`, `build`, `run`, `eval`, `debug`, `aibench` |
| `GMBasic/native/gmbadmin` → `gmbadmin` | **CLI amministrativo C** (front unico `gmb`): pacchetti, pack/publish/login/register/admin, **brain/toolchain completo**, libs, setup, daemon, rag/graph, grep, convert/convfmt, hf/mlx |
| `Notebook/build/gmb_notebook` | notebook server (C): sessione BASIC via HTTP su 127.0.0.1 |

## 1. Comandi della REPL (70 nella mappa C#)

Tutti presenti nella REPL C con lo stesso nome; `AIBENCH` (unico assente dopo F6b) è stato
riportato e usa `gmbcs aibench`. `NOTEBOOK` non era **mai registrato** nella mappa C# (codice
morto): la funzione è ripristinata come progetto autonomo `Notebook/`.

Comandi riportati nel mondo C o instradati (vedi commit F3/F6b):

| comando | dove |
|---|---|
| `FIXTURES` | runtime C `gmb_fixtures_*` (anche `gmb fixtures`, tool C `gmbadmin`) |
| `SETUP` | **C**: `gmb setup LIBS\|OCR\|TORCH\|BROWSER\|OBSCURA\|ALL` (manifest + `gmb_fetch`) |
| `LLM` | runtime C (AIPATH) |
| `ADD`/`REMOVE`/`BUMP`/`RESTORE`/`UPDATE`/`ROLLBACK`/`TRUST`/`CACHE` | **`gmbadmin pkg …`** (tool C, runtime `gmb_pkg_*`; la REPL non linka più gmb_pkg) |
| `AIBENCH` | `gmbcs aibench` |
| `HF`/`MLX` | `gmb hf` / `gmb mlx` — **C** (HF API + `gmb_fetch`, token da `~/.cache/huggingface/token`) |
| `RAG` | `gmb rag build/update/status/query` — **C** (`gmb_rag_index_source`/`search`, `--prune` con `gmb_rag_list_sources`) |
| `GRAPH` | **C**: `gmb graph build/update/status` (`gmb_graph_build`); REPL `rpl_graph_build` resta per il comando interattivo |
| `BRAIN …` | **C**: `gmbadmin brain` (install/uninstall/resolve/stats/verify/create/register/buildvault/optimize/rename/backup/export/sign/dedupe/extract/merge/convert/diff/doctor/forget) |
| `LIBS UPDATE/CHECK/PUBLISH` | **C**: `gmb libs status/ensure/check/publish` |
| `CONVERT` / `CONVFMT` | **C**: `gmb convert` / `gmb convfmt` (`gmb_doc_*`) |
| `BSP <dir>` | **C**: `gmb_bsp_compile_full` + `gmb dump chunk` → `<pagina>.bsp.chunk` |
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
| `-m`/`--managed` | **engine C#**: `gmbcs run --engine il` (con `--engine=csharp`); senza engine, errore esplicito. `gmb <file> -m` equivalente |
| `-n`/`--native` | **engine C#**: `gmbcs run --engine native` (NativeAOT via `IlProgramCompiler.CompileNative` + ilc, poi esegue); `gmbcs build --target native` per il solo binario. `gmb <file> -n` equivalente |
| `-full` | **implementato**: `gmbcs build --target il --full` (copia il runtime .NET accanto all'eseguibile; `gmb <file> -full` lo inoltra). `--all` disattiva il filtro ClosureSlim |
| `--debug-tcp <port>` | **mai stato operativo**: nel vecchio runner il valore era solo salvato (`DebugPort`), nessun listener TCP veniva aperto (verificato nel sorgente storico). Nel mondo C il flag dà un avviso esplicito; per il debug usare `IDE` o `gmbcs debug` |
| `--target wasm`, `-o` | **rimossi** con la REPL C# (POC): messaggio esplicito; per il C usare `gmb dump c` |
| `--no-optimize` | C: nessun ottimizzatore (avviso esplicito); engine C#: non esposto |

## 3. CLI `gmb` (comandi amministrativi)

Tutti in **C** (`GMBasic/native/gmbadmin`): `init`, `pack`, `keygen`, `bump`, `add`, `remove`,
`login`, `register`, `logout`, `cache`, `admin`, `pkg`, `brain` (toolchain completo),
`fixtures`, `publish` (file e `--repo`), `restore`, `update`, `trust`, `rollback`, `libs`,
`setup`, `daemon`, `rag`, `graph`, `grep`, `convert`, `convfmt`, `hf`, `mlx`.
Il CLI C# `GMBasicSharp/Cli` è stato **rimosso** (F9d): non esiste più alcun comando che
viva solo nel mondo C#.

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

1. ~~NativeAOT `-n`~~ — **portato** in `gmbcs build --target native` / `run --engine native`
   (DLL managed temporanea + `NativeBackend.Build` → eseguibile senza runtime .NET).
   Il percorso nativo canonico resta il compilatore C (`gmb build`).
2. ~~`-full`~~ — **portato** in `gmbcs build --full` (runtime .NET copiato; verificato: 314 file
   nella cartella di build, `System.Private.CoreLib` incluso).
3. ~~`--debug-tcp`~~ — **non era una funzione**: il flag veniva solo memorizzato, nessun server
   TCP esisteva (né in C# né in C). Oggi risponde con un avviso esplicito.
4. ~~GPM mode sul runner C99~~ — **fatto**: `gmb --gpm auto|always|linear` arriva all'emettitore
   C99 (`gmb_c99_set_gpm_mode`); REPL `-gpm`/`GPM` lo propagano a `RUN`/`COMPILE`.
5. **Notebook**: ripristinato come progetto C (`Notebook/`) con `/execute`, `/vars`, `/cells`,
   `/order`, `/runall`, `/reset`, `/health`, `/shutdown`, `%md`; `%sql`/`%ai`/`%vdb`/`%chart`,
   `POST /ai` (SSE) e `GET /` (frontend) **non sono supportati** e rispondono con errore
   esplicito. Il kernel usa il valutatore BASIC del runtime (niente compilazione per-cell).
6. ~~BSP index.bsp~~ — **risolto** (2026-11-02): la causa non era il front-end:
   - il `.bas` generato veniva scritto in `/tmp`, e la discovery del compilatore (ricorsiva
     sulla cartella dell'entry) includeva `.inc` estranei lasciati lì (duplicati `EscapeHtml`).
     Ora `BSP` usa una **cartella temporanea privata** (`mkdtemp`);
   - `REPLACE$` mancava in **VM/C** (A1): aggiunta (via `gmb_str_replace`, MiniTest
     `strings/replace_dollar.bas` verde su 4 backend);
   - il compilatore A1 non mappava i nomi `AI.AGENTLIST/AGENTEVENTS/AGENTGET/MEMORYASK/
     WORKFLOW*` (implementati nel VM): aggiunti. `BSP` su `shared/webapp/pages` → **2 OK, 0 FAIL**.
   Semantica di `REPLACE$` con **old vuoto**: **inserisce** il nuovo in ogni posizione (inizio
   e fine inclusi), su TUTTI i backend (allineata il 2026-11-02 in `StringHelpers.Replace`;
   .NET `string.Replace` lancia, quindi non si delega più a lui). MiniTest
   `strings/replace_dollar.bas` copre anche questo caso.
