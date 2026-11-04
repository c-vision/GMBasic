# GMBasic — Manuale completo — Indice (F1)

> Bozza di lavoro (2026-11-01). Non è il front matter del libro: fissa capitoli, ordine, fonti e
> stima pagine. Ogni capitolo ha almeno una fonte primaria nel vault.
> Stima: **1 pagina = 3.200 byte** di nota di partenza (prosa + listati), minimo 4 pagine per
> capitolo; la reference è stimata a parte (200 pagine per 2.069 voci, generata in F4).

**Due volumi** (7 × 10", KDP):

| Volume | Contenuto | Pagine stimate |
|---|---|---|
| **Vol I — Linguaggio e runtime** | Parti I-IV + appendice costanti (26 capitoli) | **407** |
| **Vol II — Applicazioni e AI + reference** | Parti V-VI + reference 1.540 funzioni + glossario | **514** |

## Parte I — Introduzione

| Cap. | Titolo | pp | Note | Esempi (presenti/citati) |
|---|---|---|---|---|
| I.1 | Installazione e primo programma | 8 | 3 | 0/3 |
| I.2 | REPL, IDE e Notebook | 23 | 5 | 2/12 |
| I.3 | Sintassi e struttura del programma | 4 | 3 | 12/13 |

## Parte II — Il linguaggio

| Cap. | Titolo | pp | Note | Esempi (presenti/citati) |
|---|---|---|---|---|
| II.4 | Grammatica e lessico | 12 | 5 | 10/10 |
| II.5 | Tipi, variabili e scope | 17 | 6 | 51/51 |
| II.6 | Operatori ed espressioni | 4 | 1 | 6/6 |
| II.7 | Controllo di flusso | 6 | 3 | 9/9 |
| II.8 | Classi e oggetti | 10 | 2 | 4/4 |
| II.9 | Array, collezioni e stringhe | 24 | 5 | 86/88 |
| II.10 | Compatibilità Clipper/xBase | 15 | 6 | 29/31 |
| II.11 | Moduli, preprocessore e librerie | 4 | 2 | 3/4 |

## Parte III — Runtime e strumenti

| Cap. | Titolo | pp | Note | Esempi (presenti/citati) |
|---|---|---|---|---|
| III.12 | Dentro il runtime | 37 | 6 | 1/10 |
| III.13 | GPM e concorrenza | 24 | 6 | 68/68 |
| III.14 | Debug e diagnostica | 4 | 2 | 2/3 |
| III.15 | Testare i propri programmi | 16 | 3 | 9/16 |
| III.16 | FFI e librerie native | 15 | 3 | 28/29 |

## Parte IV — Applicazioni

| Cap. | Titolo | pp | Note | Esempi (presenti/citati) |
|---|---|---|---|---|
| IV.17 | Console e TUI | 16 | 7 | 26/27 |
| IV.18 | Colori, stili e temi | 5 | 5 | 5/6 |
| IV.19 | Finestre ed eventi | 5 | 4 | 7/7 |
| IV.20 | Controlli utente | 46 | 19 | 59/72 |
| IV.21 | Grafica, suono e giochi | 9 | 4 | 33/33 |
| IV.22 | File, I/O e documenti | 24 | 8 | 73/76 |
| IV.23 | Database | 31 | 12 | 101/106 |
| IV.24 | Rete e web | 34 | 7 | 29/37 |
| IV.25 | Messaggistica e code | 7 | 8 | 9/15 |
| IV.26 | Sicurezza e crittografia | 7 | 3 | 32/32 |

## Parte V — Calcolo e intelligenza artificiale

| Cap. | Titolo | pp | Note | Esempi (presenti/citati) |
|---|---|---|---|---|
| V.27 | Matematica e statistica | 28 | 16 | 58/58 |
| V.28 | Segnali, trading e blockchain | 12 | 8 | 33/33 |
| V.29 | Date, ore e sistema | 10 | 4 | 40/40 |
| V.30 | Compressione e dati binari | 4 | 2 | 14/14 |
| V.31 | TORCH | 8 | 2 | 5/5 |
| V.32 | AI: provider, modelli e memoria | 54 | 9 | 113/131 |
| V.33 | Agenti e workflow | 23 | 11 | 40/44 |
| V.34 | RAG, vettori e Second Brain | 32 | 5 | 39/40 |
| V.35 | LLM locale | 81 | 12 | 94/102 |
| V.36 | Computer use e browser | 18 | 3 | 53/59 |

## Parte VI — Distribuzione

| Cap. | Titolo | pp | Note | Esempi (presenti/citati) |
|---|---|---|---|---|
| VI.37 | Package manager | 26 | 5 | 8/15 |
| VI.38 | Configurazione e percorsi | 9 | 2 | 3/3 |

## Appendici

| Cap. | Titolo | pp | Note | Esempi (presenti/citati) |
|---|---|---|---|---|
| A1 | Reference delle 1.540 funzioni | 200 | 0 | 0/0 |
| A2 | Costanti globali e tipi | 4 | 1 | 3/4 |

## Fonti per capitolo

`capitolo` = fonte primaria, `sezione` = fonte secondaria, `fuso` = variante quasi-duplicata
assorbita (non si stampa due volte).

### I.1 — Installazione e primo programma

- `CLI-Flags` — capitolo
- `Compiler-CLI-Guide` — sezione
- `Setup-Infrastructure` — sezione

### I.2 — REPL, IDE e Notebook

- `REPL-Commands-Reference` — capitolo
- `Brain-Toolchain-Guide` — sezione
- `GMBasicIDE-Guida-Utente` — sezione
- `Notebook-User-Guide` — sezione
- `OutputCapture` — sezione

### I.3 — Sintassi e struttura del programma

- `Sintassi-Base-Guida-Utente` — capitolo
- `FUNCTION-Main-ExitCode-Guida-Utente` — sezione
- `Modulo-Livello-TopLevel-Guida-Utente` — sezione

### II.4 — Grammatica e lessico

- `Grammar` — capitolo
- `Direttiva-Sintassi-AGENTS` — sezione
- `ImplicitLineNumbers` — sezione
- `Line-Number-Spec` — sezione
- `Polimorfismo-Guida-Utente` — sezione

### II.5 — Tipi, variabili e scope

- `Variables-Scope-Spec` — capitolo
- `DirectCast-Guida-Utente` — sezione
- `NullConditionalAccess-Guida-Utente` — sezione
- `TYPEINFO-Riflessione-Guida-Utente` — sezione
- `Types-NamedTuple-Guida-Utente` — sezione
- `Variables-Scope-Guida-Utente` — sezione

### II.6 — Operatori ed espressioni

- `XOR-AND-OR-SWAP-Guida-Utente` — capitolo

### II.7 — Controllo di flusso

- `Loops-FOR-Guida-Utente` — capitolo
- `Goto-Gosub-OnError-Guide` — sezione
- `SelectCase-IS-Comparazione-Guida-Utente` — sezione

### II.8 — Classi e oggetti

- `Classes-User-Manual` — capitolo
- `Class-Static-Fields-Guida-Utente` — sezione

### II.9 — Array, collezioni e stringhe

- `Collezioni-Guida-Utente` — capitolo
- `Arrays-Guida-Utente` — sezione
- `Conversion-Guida-Utente` — sezione
- `Stringhe-Guida-Utente` — sezione
- `Strings` — sezione

### II.10 — Compatibilità Clipper/xBase

- `Clipper-Macro-Eval-Blocks-Compat` — capitolo
- `Clipper-Macro-Eval-Blocks-Guida-Utente` — sezione
- `ClipperLib-Clipper-5.3-Reference` — sezione
- `DYNAMIC-Guida-Utente` — sezione
- `GET-Guida-Utente` — sezione
- `GET-Tipi-Guida-Utente` — sezione

### II.11 — Moduli, preprocessore e librerie

- `LIBRARY-gmblib-Guide` — capitolo
- `Librerie-Importate-Guida-Utente` — sezione
- `PrecompilerMultiTarget` — sezione

### III.12 — Dentro il runtime

- `C99-GPM-Technical-Manual` — capitolo
- `Executor-Architecture` — sezione
- `GM-Basic-BRT-Reference-Card` — sezione
- `PRG-Format` — sezione
- `VM-C-GPM-Technical-Manual` — sezione
- `VM-C-Opcode-Spec` — sezione

### III.13 — GPM e concorrenza

- `PCodeVM-GPM-User-Manual` — capitolo
- `ATOMIC-Guida-Utente` — sezione
- `Concorrenza-GPM-Guida-Utente` — sezione
- `DOEVENTS-Guida-Utente` — sezione
- `Defer-Guida-Utente` — sezione
- `Http-FetchInto-Guida-Utente` — sezione

### III.14 — Debug e diagnostica

- `Debug-Error-Workflow` — capitolo
- `Debug-Unificato-E-Backend` — sezione

### III.15 — Testare i propri programmi

- `Testing-Guide` — capitolo
- `Test-Index-and-Runners` — sezione
- `Test-Run-All-Guide` — sezione

### III.16 — FFI e librerie native

- `FFI-Guida-Utente` — capitolo
- `Resources-Native-Libs-SDL` — sezione
- `FFI-Guida-Utente` — fuso

### IV.17 — Console e TUI

- `Console-Architecture` — capitolo
- `Clipper-TUI-Manual` — sezione
- `Console-Architecture-Details` — sezione
- `PetsciiControlChars` — sezione
- `TBROWSE-Sort-Filter-Guida-Utente` — sezione
- `TBrowse-Reference` — sezione
- `TEXTMODE-Guida-Utente` — sezione

### IV.18 — Colori, stili e temi

- `Palette-System` — capitolo
- `Palette-Colore-Guida-Utente` — sezione
- `STYLE-Guida-Utente` — sezione
- `STYLE-primary-Guida-Utente` — sezione
- `THEME-Guida-Utente` — sezione

### IV.19 — Finestre ed eventi

- `Window-Management-Event-System` — capitolo
- `Finestre-Eventi-Guida-Utente` — sezione
- `Tray-Guida-Utente` — sezione
- `WINDOW-SHOW-HIDE-Guida-Utente` — sezione

### IV.20 — Controlli utente

- `Controlli-Pixel-Proprieta-Metodi-Guida-Utente` — capitolo
- `CandleChart-Controllo-Guida-Utente` — sezione
- `ClipperLib-Controls-Reference` — sezione
- `ContextMenu-Controllo-Guida-Utente` — sezione
- `Controlli-Utente-Base-Control` — sezione
- `Controlli-Utente-Composizione` — sezione
- `Controlli-Utente-Struttura` — sezione
- `Controlli-Utente-UserControl` — sezione
- `Controls-CHART-Guida-Utente` — sezione
- `DataGrid-Controllo-Guida-Utente` — sezione
- `MarketDepth-Controllo-Guida-Utente` — sezione
- `OrderTicket-Controllo-Guida-Utente` — sezione
- `PANEL-Guida-Utente` — sezione
- `SPLITTER-CONTEXTMENU-Builtin-Guida-Utente` — sezione
- `Segmented-Controllo-Guida-Utente` — sezione
- `Splitter-Controllo-Guida-Utente` — sezione
- `Trading-Minori-Controlli-Guida-Utente` — sezione
- `Watchlist-Controllo-Guida-Utente` — sezione
- `Clipper-Controls-Reference` — fuso

### IV.21 — Grafica, suono e giochi

- `Graphics-Guide` — capitolo
- `Graph-Guida-Utente` — sezione
- `Graph-FFI-Guida-Utente` — sezione
- `Joy-Guida-Utente` — sezione
- `SoundEnhanced` — sezione

### IV.22 — File, I/O e documenti

- `IO-Guida-Utente` — capitolo
- `BSP-Guide` — sezione
- `CONVERT-User-Manual` — sezione
- `Convert-Setup-User-Manual` — sezione
- `DOM-Guida-Utente` — sezione
- `Document-Processing` — sezione
- `GREP-Guida-Utente` — sezione
- `Json-Guida-Utente` — sezione

### IV.23 — Database

- `DBF-Guida-Utente` — capitolo
- `Database-Guida-Utente` — sezione
- `Dbf-UpdateOn-Guida-Utente` — sezione
- `DuckDB-Guida-Utente` — sezione
- `ISAM-Guida-Utente` — sezione
- `LEDGER-Guide` — sezione
- `MongoDB-Guida-Utente` — sezione
- `RDD-Guide` — sezione
- `SPEED-Guide` — sezione
- `SQL-Guide` — sezione
- `DBF-Guide` — fuso
- `ISAM-Guide` — fuso

### IV.24 — Rete e web

- `Http-Mvc-Programmer-Manual` — capitolo
- `HTTP-Streaming-Guida-Utente` — sezione
- `Load-Balancing-Guida-Utente` — sezione
- `Mvc-Http-Guida-Utente` — sezione
- `Mvc-Routing-Conventions` — sezione
- `Net-Guida-Utente` — sezione
- `WebDashboard-Guide` — sezione

### IV.25 — Messaggistica e code

- `Messaging-Guida-Utente` — capitolo
- `AMQP-Guida-Utente` — sezione
- `Kafka-Guida-Utente` — sezione
- `MQTT-Guida-Utente` — sezione
- `NATS-Guida-Utente` — sezione
- `Protobuf-Guida-Utente` — sezione
- `Redis-Guida-Utente` — sezione
- `gRPC-Guida-Utente` — sezione

### IV.26 — Sicurezza e crittografia

- `Security-Best-Practices` — capitolo
- `Cryptography-Guida-Utente` — sezione
- `Security-Middleware-Reference` — sezione

### V.27 — Matematica e statistica

- `Math-Guida-Utente` — capitolo
- `ABS-Guida-Utente` — sezione
- `Analysis-Manual` — sezione
- `BITS-Guida-Utente` — sezione
- `Finance-Guida-Utente` — sezione
- `Funzioni-Matematiche-Guida-Utente` — sezione
- `KdTree-Manual` — sezione
- `Matrix-Linalg-Manual` — sezione
- `Optimize-Manual` — sezione
- `Optimize-OR-Guida-Utente` — sezione
- `Optimize-ORTools-Guida-Utente` — sezione
- `Spatial-Guida-Utente` — sezione
- `Special-Manual` — sezione
- `Stats-Manual` — sezione
- `Trig-Guida-Utente` — sezione
- `Vector-Manual` — sezione
- `Spatial-Manual` — fuso
- `Stats-Guida-Utente` — fuso

### V.28 — Segnali, trading e blockchain

- `Signal-Manual` — capitolo
- `Blockchain-Guida-Utente` — sezione
- `Signals-Guida-Utente` — sezione
- `TimeSeries-Guida-Utente` — sezione
- `Trading-Bonds-Guida-Utente` — sezione
- `Trading-Guida-Utente` — sezione
- `Trading-Guida-Utente` — sezione
- `Signal-Guida-Utente` — fuso

### V.29 — Date, ore e sistema

- `System-Guida-Utente` — capitolo
- `DataOra-Guida-Utente` — sezione
- `Datetime-Guida-Utente` — sezione
- `System-SYS-Memoria-Guida-Utente` — sezione

### V.30 — Compressione e dati binari

- `COMPRESSION-Guida-Utente` — capitolo
- `Compression-Guida-Utente` — fuso

### V.31 — TORCH

- `Torch-Manual` — capitolo
- `Torch-Guida-Utente` — fuso

### V.32 — AI: provider, modelli e memoria

- `AI-Namespace-Manual` — capitolo
- `AI-Documenti-Guide` — sezione
- `AI-Immagini-Guide` — sezione
- `AI-Memory-Guide` — sezione
- `AI-OCR-Audio-Guide` — sezione
- `AI-Providers-Guide` — sezione
- `AI-Skills-Plugins-MCP-Guide` — sezione
- `ASR-Qwen3-ASR-Guida-Utente` — sezione
- `Connector-Runtime-Guida-Utente` — sezione

### V.33 — Agenti e workflow

- `Agents-Manual` — capitolo
- `AI-AGENT-CodeGen-Guida-Utente` — sezione
- `AgentCodeGen-Guide` — sezione
- `AgentIngestion-Guide` — sezione
- `AgentL5-Guide` — sezione
- `Agents-Dashboard-Guida-Utente` — sezione
- `RecursiveMAS-Manual` — sezione
- `SelfConsistency-Guida-Utente` — sezione
- `Workflow-Guida-Utente` — sezione
- `Workflow-Guide` — sezione
- `WorkflowDag-Guide` — sezione

### V.34 — RAG, vettori e Second Brain

- `AI-RAG-Guide` — capitolo
- `AI-SemanticGraph-Guida-Utente` — sezione
- `Rag-Eval-Guida-Utente` — sezione
- `Second-Brain-Guide` — sezione
- `VECTORDB-Guide` — sezione

### V.35 — LLM locale

- `LLM-Arch-ModelCompiler-Guida-Utente` — capitolo
- `AI-Distill-Guida-Utente` — sezione
- `Embedded-LLM-Infrastruttura-Guida` — sezione
- `GMBasic-Modelli-AI-Locali` — sezione
- `KV-Quant-Hetero-KV-Guida-Utente` — sezione
- `LLM-Server-Guida` — sezione
- `LLM-Server-OpenAI-Compatibile` — sezione
- `LLM-Tensor-Compare-Dense-Guida-Utente` — sezione
- `LLM-Tokens-Guida-Utente` — sezione
- `LLVM-Modifica-LLM-Flusso-Completo-Guida-Utente` — sezione
- `Modelli-IA-Locali` — sezione
- `TRAIN-Namespace-Guide` — sezione

### V.36 — Computer use e browser

- `Browser-Automation-Guida-Completa` — capitolo
- `Browser-Automation-Plugin` — sezione
- `CUA-Universal-Action-Runtime-Guida` — sezione

### VI.37 — Package manager

- `PackageManager-Manual` — capitolo
- `PackageManager-Publish-Contributor-Guide` — sezione
- `PackageManager-Publish-Ufficiali-Guide` — sezione
- `PackageManager-User-Guide` — sezione
- `Package-Manager-Guida-Utente` — fuso

### VI.38 — Configurazione e percorsi

- `Options-Guida-Utente` — capitolo
- `Paths-Config-User-Guide` — sezione

### A1 — Reference delle 1.540 funzioni


### A2 — Costanti globali e tipi

- `Costanti-Globali-Guida-Utente` — capitolo

### A3 — Glossario

Appendice generata in F2 dal file `docs/manual/glossario.md`: i termini canonici del libro con le forme da non usare. Non ha note nel vault: il testo è editoriale, non deriva da una guida.

## Fuori perimetro (non si stampano)

Materiale di lavoro del progetto: non appartiene a un manuale d'uso.

| Nota | Perché è esclusa |
|---|---|
| `Commands-Generation-Guide` | materiale di sviluppo: tooling dei generatori |
| `Cross-Repo-Independence-Rule` | regola di processo per gli agenti |
| `Cross-Repo-Verification-Rule` | regola di processo per gli agenti |
| `Event-System-Plan` | piano di lavoro interno |
| `GM-Basic-IDE-LookAndFeel-Review` | review di design dell'IDE |
| `Generatori-Guida-Utente` | materiale di sviluppo: generazione della documentazione |
| `Graphic-Test-Runners` | runner di test grafici (materiale di sviluppo) |
| `Graphify-Integration` | integrazione di una skill interna |
| `IL-Graphics-Input` | dettaglio implementativo del backend IL |
| `Loop-Fixes-Architecture` | architettura storica dell'esecutore |
| `Repo-Legacy-Referenza` | archivi storici, nessuna relazione col prodotto |
| `SQL-ILBackend-RealDB-Testing` | procedura di test contro database reali |
| `Test-Exec-Usage` | runner legacy |
| `Test-Guide-C99-OpenSSL` | procedura di test del transpiler |
| `Test-Procedure-AST-DASM` | procedura di test interna |
| `Test-Procedure` | procedura di test interna |
| `Test-Runner-Implementation` | dettaglio implementativo dei runner |
| `Test-Sample-Classification` | classificazione dei sample per la CI |
| `_Index` | indice delle guide: mappa di lavoro, non capitolo |

## Totale

- **41 capitoli** (39 di testo + reference generata + glossario), **218 note** in capitoli, **10 varianti assorbite**.
- **921 pagine**: corpo 712 + costanti/tipi 4 + glossario 5 + reference 200.
- Esempi nei capitoli stampati: **1312** citati, **1194** esistenti nel repo.
- 18 note di materiale interno restano fuori (tabella sopra); `_Index` è la mappa usata per F1, non un capitolo.
