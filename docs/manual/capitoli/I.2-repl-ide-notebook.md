# I.2 — REPL, IDE e Notebook

## Cosa ottieni da questo capitolo

Tre modi di lavorare senza ricompilare un progetto intero: la REPL per provare un'istruzione,
l'IDE per navigare il codice con il debugger, il Notebook per alternare testo, codice e output.
Sai quale usare per cosa e quali comandi cercare.

## 1. Tre superfici, un solo linguaggio

| Superficie | Cos'è | Quando usarla |
|---|---|---|
| **REPL** | ciclo leggi-valuta-stampa su terminale | provare una riga, ispezionare un programma, compilare un sample |
| **IDE** | ambiente integrato con editor, esecuzione e debugger | scrivere e correggere un programma intero |
| **Notebook** | documento a celle con output inline | spiegare, confrontare varianti, tenere insieme testo e risultati |

Nessuna delle tre inventa sintassi: quello che scrivi è BASIC, con le regole del capitolo I.3.
Cambia il contenitore, non il linguaggio.

## 2. La REPL

Si avvia senza argomenti — `gmb` con il binario installato, `dotnet run --project GMBasicSharp/Cli`
da un checkout — e lavora in tre modalità:

| Modalità | Come | Cosa fa |
|---|---|---|
| interattiva | nessun file | esegue i comandi che digiti, uno per volta |
| batch | `gmb programma.bas` | esegue un file e termina |
| debug remoto | `--debug-tcp <porta>` | espone un server DAP per un debugger esterno |

Per l'automazione c'è `--run <comando>`: esegue **un comando della REPL** all'avvio e non resta
in attesa. La distinzione conta: `--run` non prende istruzioni BASIC.

## 3. I comandi, per famiglie

La REPL non ha un solo elenco: i comandi sono raggruppati per mestiere. Questi sono quelli che
servono nei primi giorni.

| Famiglia | Comandi | Cosa fanno |
|---|---|---|
| editor | `LIST`, `NEW` | mostrano o azzerano il programma in memoria |
| compilazione | `COMPILE`, `RUN`, `LOADPRG`, `PRGINFO` | producono un chunk, eseguono un `.bas` o un `.prg`, leggono l'intestazione di un chunk |
| ispezione | `AST`, `DASM`, `IL` | albero sintattico, disassemblaggio, dump IL (quest'ultimo solo se l'engine C# è presente) |
| valutazione | `EVAL` | valuta uno snippet BASIC |
| runtime | `SETUP`, `LIBS`, `FIXTURES` | stato della cache delle librerie, bundle nativi, dati dei sample |
| modelli | `LLM` | elenca i modelli presenti sotto `AIPATH` |
| pacchetti | `ADD`, `REMOVE`, `BUMP`, `TRUST`, `CACHE`, `RESTORE`, `UPDATE`, `ROLLBACK` | gestiscono `gmb-package.json`, chiavi, cache e lock |
| AI e dati | `RAG`, `GRAPH`, `BRAIN` | costruiscono indici e grafi, collegano un Second Brain |

Il comando `RUN` accetta il backend: `RUN file.bas` usa il percorso p-code, `RUN file.bas -m`
il managed, `-n` il nativo, `-c99` il backend C99Native. La scelta del backend è quindi una
proprietà del comando, non del file.

> **Attenzione a `EVAL`.** Nei due motori il nome ha due significati: nella REPL C valuta uno
> snippet BASIC, nella REPL C# esegue l'*Eval Harness* dei modelli. Finché i due motori convivono,
> per lo snippet usa la forma esplicita `gmb eval "<istruzioni>"`, che è quella verificata qui sotto.

## 4. Un esempio eseguito

Questa è una prova reale, non una trascrizione: il CLI del compilatore C valuta lo snippet e
stampa il risultato.

```sh
GMBasic/native/gmb_compiler_c/build/gmb eval "PRINT 3+4"
```

```
chunk -> /tmp/gmb_eval_6267.chunk (271 bytes)
 7
```

La prima riga dice che lo snippet è stato compilato in un chunk (il formato eseguibile del
percorso p-code, capitolo III.12); la seconda è l'output del programma.

E questa è la prova di ciò che **non** funziona, utile perché l'errore è chiaro:

```sh
gmbrepl --run "PRINT 3+4"
```

```
?comando sconosciuto: PRINT (HELP per l'elenco)
```

`--run` vuole un comando della REPL, non un'istruzione BASIC: la sintassi del linguaggio non
passa da lì.

## 5. L'IDE

Nel repository ci sono due cose diverse che si chiamano IDE, e la distinzione evita una delusione:

| Cosa | Dove sta | Stato |
|---|---|---|
| **IDE TUI della REPL** | dentro il repository: `REPL/ide.c` (REPL C canonica), logica condivisa in `gmb_runtime_c/src/gmb_ide.c` | disponibile, si apre dalla REPL con il comando `IDE` |
| **IDE grafici** (GUI nativa in stile VB6, IDE TUI scritto in BASIC) | progetti separati, **non** in questo repository | in sviluppo, esclusi finché incompleti |

Se stai cercando una finestra con i pulsanti e non la trovi, è la seconda riga della tabella:
non è ancora distribuita. La TUI della REPL invece c'è, ed è quella che apre `IDE`.

Per l'editing da un editor esterno il progetto offre un **language server** separato,
`GMBasic/native/gmb_compiler_c/build/gmblsp`, che parla LSP su stdio: lo avvia l'editor, non la
REPL (che si limita a rimandare a quel tool).

## 6. Il Notebook

Un notebook è un file `.gmnb` fatto di celle. Ogni cella è codice BASIC oppure una delle celle
speciali, che iniziano con `%`:

| Cella | Cosa produce |
|---|---|
| `%md` | testo e documentazione formattati |
| `%sql` | interrogazione di un database, con risultato tabellare |
| `%ai` | risposta di un modello |
| `%vdb` | ricerca semantica nel notebook |
| `%chart` | grafico dai dati della cella |

Il kernel è un server HTTP per cella: l'esecuzione di una cella non richiede di rieseguire le
altre, ma le variabili definite prima restano disponibili secondo l'ordine di esecuzione.

Il notebook si salva in `.gmnb` e si esporta in tre forme: programma BASIC (`.bas`), documento
HTML, PDF. L'esportazione in `.bas` è il ponte verso il resto del manuale: quello che hai
prototipato in celle diventa un programma eseguibile.

Cosa il Notebook non fa, per evitare di cercarlo: non è un debugger, non sostituisce l'IDE per i
progetti grandi e non esegue le celle in parallelo.

## 7. Comandi e simboli di questo capitolo

| Nome | Forma | Note |
|---|---|---|
| `gmb` | eseguibile | CLI e REPL in un solo entry point |
| `--run <comando>` | flag | esegue un comando della REPL e termina |
| `--debug-tcp <porta>` | flag | server di debug DAP |
| `EVAL` | comando REPL | snippet BASIC; ambigua fra i due motori |
| `gmb eval "<istruzioni>"` | CLI | forma esplicita e verificata |
| `RUN`, `COMPILE`, `PRGINFO`, `DASM` | comandi REPL | esecuzione e ispezione |
| `IDE` | comando REPL | apre la TUI integrata |
| `%md %sql %ai %vdb %chart` | celle Notebook | testo, SQL, modello, vettori, grafici |

## 8. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| `?comando sconosciuto: PRINT` | istruzione BASIC passata a `--run` | `--run` prende comandi REPL; per BASIC usa `gmb eval` |
| `EVAL` fa partire un benchmark di modelli | sei nella REPL C#, dove `EVAL` è l'Eval Harness | usa `gmb eval "<istruzioni>"` |
| `IDE` non apre nulla | manca il binario TUI o il terminale non è interattivo | avvia la REPL in un terminale vero, non in una pipe |
| il Notebook non trova una variabile | la cella che la definisce non è stata eseguita prima | esegui le celle nell'ordine giusto |
| `gmblsp` non parte dall'editor | percorso non configurato | indica il binario in `GMBasic/native/gmb_compiler_c/build/` |

## 9. Dove andare adesso

- **I.3** — sintassi e struttura, per scrivere codice che queste superfici eseguono.
- **III.14** — debug e diagnostica: breakpoint, ispezione, log dei backend.
- **III.15** — testare i propri programmi con i MiniTest.
- **VI.38** — configurazione e percorsi: dove stanno cache, impostazioni e provider.



<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### REPL-Commands-Reference

*Fonte: `REPL-Commands-Reference`*


## Motore C (REPL unica in costruzione) — comandi che pilotano il compilatore
| comando | cosa fa (motore C) | comando CLI usato |
|---|---|---|
| `AST <file.bas>` | AST + riepilogo linker | `gmb dump ast <file>` |
| `AST ALL\|ERR\|STOP` | catalogo dei sample | `gmb dump ast` per ogni file |
| `DASM <file.bas>` | disassemblaggio del bytecode | `gmb dump disasm <file>` |
| `DASM <file.chunk>` | disassemblaggio di un chunk esistente — **engine C#**: P-Code del sorgente o di un `.prg` | `gmb dump disasm` · `gmbcs dump disasm` |
| `PRGINFO <file.chunk>` | header del chunk A1/GMBC — **engine C#**: `N opcodes, M constants` di un `.prg` | `gmb_chunk` · `gmbcs prginfo` |
| `PRGINFO ALL\|ERR\|STOP` | catalogo: compila ogni sample | `gmb dump chunk` |
| `COMPILE <file.bas>` | chunk (VMC) o C (C99) — **engine C#**: P-Code `.prg` (VMC) o eseguibile IL (C99) | `gmb dump chunk\|c --out` · `gmbcs build --target pcode\|il` |
| `RUN <file.bas>` | compila e esegue (VMC) | `gmb dump chunk` + `gmb_vm_host` · engine C#: `gmbcs run --engine pcode` |
| `RUN <file.bas>` con `-c99` | backend C99Native | `gmb run --project <dir> --entry <file>` · engine C#: `gmbcs run --engine il` |
| `EVAL <istruzioni>` | valuta uno snippet BASIC | `gmb eval "<istruzioni>"` |
| `PCODE <file.bas>` | disassemblaggio P-Code (solo engine C#) | `gmbcs dump pcode` |
| `IL <file.bas>` | dump IL — **solo se l'engine C# è presente** (`gmbcs dump il`) | `gmbcs` (F4) |
| `FIXTURES [LIST\|PRESENT\|ENSURE\|PULL <nome>]` | fixture dei dati (manifest + cartella + presenti) | runtime C `gmb_fixtures_*` |
| `SETUP` | stato della cache delle librerie native (RID, cartella `~/.gmbasic/libs/<rid>`, n. file) | runtime C (`gmb_paths_libs_cache_dir`, `gmb_libs_rid`) |
| `LLM` | elenco dei modelli sotto AIPATH (`modelPaths` di `ai.json`) | runtime C (paths) |
| `ADD <nome[@ver]> [--dev] [--dir d]` | aggiunge/aggiorna una dipendenza nel `gmb-package.json` | `gmb_pkg_manifest_set_dep` |
| `REMOVE <nome> [--dir d]` | rimuove la dipendenza (dependencies + devDependencies) | `gmb_pkg_manifest_remove_dep` |
| `BUMP <major\|minor\|patch\|X.Y.Z> [--dir d]` | incrementa `version` nel manifest | edit del manifest (runtime C) |
| `TRUST [LIST \| ADD <nome> <chiave\|file.pem> \| REMOVE <nome> \| FINGERPRINT <chiave\|file.pem>]` | keyring dei pacchetti (`~/.gmbasic/trusted-keys.json`, lo stesso del PM C#) | `gmb_pkg_keyring_*`, `gmb_pkg_key_fingerprint` |
| `CACHE [LIST \| GC \| DEDUP \| CLEAN [--force]]` | cache pacchetti `~/.gmbasic/packages` + content-addressable `<cache>/content` | `gmb_pkg_content_gc`, `gmb_pkg_content_intern_dir` |
| `RESTORE [dir] [--locked] [--cache d] [--registry url]` | risolve le dipendenze del manifest, scarica le mancanti e scrive `gmb-package.lock` (con lock-history) | `gmb_pkg_resolve` + `gmb_pkg_lock_write_with_history` |
| `UPDATE [dir] [--check] [--cache d] [--registry url]` | mostra `installed -> latest` per ogni dipendenza e aggiorna il lock (`--check` non scrive) | resolver C |
| `ROLLBACK [dir] [--list] [--to <prefisso>] [--cache d]` | ripristina un lock dalla lock-history, archivia il corrente e re-installa | lock-history + resolver C |

## Editor Commands
| Comando | Sintassi | Descrizione |
|---|---|---|
| `LIST` | `LIST [start-end]` | Mostra il programma corrente. `LIST 10-30` mostra righe 10-30 |
| `NEW` | `NEW` | Cancella il programma in memoria |
| `AUTO` | `AUTO [start]` | Numerazione automatica righe. `AUTO 100` parte da 100, step 10 |
| `SAVE` | `SAVE "file.bas"` | Salva il programma su file |
| `LOAD` | `LOAD "file.bas"` | Carica un programma da file |
| `DELETE` / `DEL` | `DELETE 10-20` | Cancella righe 10-20 |
| `RENUM` | `RENUM [start]` | Rinumerazione righe. `RENUM 100` parte da 100, step 10 |
| `SAMPLE` | `SAMPLE <nome>` | Carica un programma d'esempio da `shared/samples/` |
| `EDIT` | `EDIT [riga]` | Apre l'editor TUI (GMBasicIde, stile QBasic) sulla riga indicata |
| `PROMPT` | `PROMPT [testo]` | Imposta il prompt del REPL (es. `PROMPT GMB!`); senza argomenti lo azzera |

## Compilazione
| Comando | Sintassi | Descrizione |
|---|---|---|
| `COMPILE` | `COMPILE "file.bas" [-m\|-n] [-dump] [-full] [-all] [-r rid] [--no-optimize] [--dump-il]` | Compila un file BASIC. `-m` = managed (DLL + apphost, richiede .NET); `-n` = eseguibile nativo AOT; `-full` (con `-m`) = distribuzione autonoma: closure copiata **e** framework .NET accanto (senza `-full` il percorso di esecuzione `-m` usa symlink, ~140 KB); `-all` = closure **completa** (di default `-m` omette CONVERT/onnx/provider SQL non raggiungibili: 47 MB invece di 262 per un `PRINT`, con `[slim] escluso: …` stampato) |
| | `-m` / `--managed` | Backend **managed**: DLL + apphost stub (richiede .NET installato) |
| | `-n` / `--native` | Backend **nativo**: singolo binario NativeAOT (Mach-O/ELF, nessun runtime .NET a carico) |
| | `-dump` | Genera `.pasm` / `.asm` |
| | `-full` | ⚠️ **legacy/D3**: includere il runtime .NET in `bin/` (self-contained) NON è più lo standard — il backend IL è **AOT** (native, zero .NET). Mantenuto solo come riferimento storico |
| | `-r <rid>` | Target runtime (`osx-arm64`, `linux-x64`, `win-x64`) |
| | `--no-optimize` | Salta ottimizzatore ILOptim |
| | `--dump-il` | Genera disassembly IL completo (`<nome>.il`) |
| `BSP` | `BSP <dir>` | Pre-compila tutte le pagine `.bsp` nella directory |
| `RUN` | `RUN [file] [-m\|-n]` | Esegui il programma corrente o un file — **default P-Code (PCodeVM interna)**; `-m`/`--managed` → DLL+apphost; `-n`/`--native` → binario nativo |
| `EXEC` | `EXEC "file.exe" [-m\|-n]` | Esegui un `.exe` già compilato (`-m`/`-n` indicano come è stato prodotto) |
| `LOADPRG` | `LOADPRG <file.prg>` | Carica un `.prg` P-Code (chunk A1/GMBC) per eseguirlo con la PCodeVM interna |
| `PRGINFO` | `PRGINFO [file.prg]` | Mostra info del `.prg` caricato (o di un file): opcodes + costanti |
| `DEPLOY` / `DEP` | `DEPLOY <file.bas> [-r <runtime>]` | Compila un file `.bas` e lo copia come eseguibile pronto al deploy |

## Debug / Analisi
| Comando | Sintassi | Descrizione |
|---|---|---|
| `AST` | `AST` | Mostra l'AST (Abstract Syntax Tree) del programma |
| `DASM` | `DASM` | Disassembla il P-Code del programma (chunk A1, PCodeVM interna) |
| `IL` | `IL` | Mostra l'IL generato dal backend nativo |
| `FLATTEN` | `FLATTEN` | Mostra la versione flatten del programma |

## Percorso GPM (backend C99)
| Comando | Sintassi | Descrizione |
|---|---|---|
| `GPM` | `GPM` | Mostra la modalità + la spiegazione dei tre modi |
| `GPM AUTO` | `GPM auto` | Lo scheduler entra solo se il programma usa la concorrenza (default) |
| `GPM ALWAYS` | `GPM always` | Forza lo scheduler anche se il rilevamento non la vede |
| `GPM LINEAR` | `GPM linear` | Percorso diretto se il programma è lineare; con concorrenza ripiega su Auto |

## Filesystem
| Comando | Alias | Sintassi | Descrizione |
|---|---|---|---|
| `LS` | — | `LS [path]` | Elenca file nella directory |
| `CD` | — | `CD <path>` | Cambia directory corrente |
| `WHERE` | `WH` | `WHERE` | Mostra directory corrente |
| `RN` | `RENAME`, `RM` | `RN "old.bas" "new.bas"` | Rinomina file |
| `DL` | `DELF` | `DL "file.ext"` | Elimina file |
| `CP` | `COPY` | `CP "src" "dst"` | Copia file |
| `DP` | `DUPLICATE` | `DP "file"` | Duplica file |
| `BL` | `BACKUP` | `BL "file"` | Backup file (copia con `.bak`) |

## Conversione Documenti
| Comando | Sintassi | Descrizione |
|---|---|---|
| `CONVERT` | `CONVERT "file.ext" [-o out.ext]` | Converti documento in markdown/html/txt |
| `CONVERT EXTRACT` | `CONVERT EXTRACT "src.pdf" TO "out.md"` | Estrai un documento (con OCR dove serve) e salvalo su file |
| `CONVERT RENDER` | `CONVERT RENDER "src.pdf" TO "out.png"` | Renderizza la pagina 0 del PDF a PNG |
| | `-o out.md` | Salva output su file (default: stdout) |
| | `-o out.html` | Output HTML |
| | `-o out.txt` | Output plain text |
| `BATCH` | `BATCH "dir/" [-r] [-o ext]` | Conversione batch di tutti i file |
| | `-r` | Ricorsivo nelle sottodirectory |
| | `-o html` | Formato output (default: md) |
| `WEB2MD` | `WEB2MD "https://..." [-o out.md]` | Converti pagina web → markdown |
| `CONVFMT` | `CONVFMT` | Elenca i formati input supportati (stesso elenco di `CONVERT.FORMATS()`) + 3 output (md, html, txt) |

### Esempi

CONVERT  "report.docx"                         → markdown a schermo
CONVERT  "data.xlsx"  -o table.html            → tabella HTML
CONVERT  "image.png"  -o desc.txt              → placeholder testuale
BATCH    "documents/" -r                        → tutti → .md ricorsivo
BATCH    "*.pdf"      -o html                   → tutti i PDF → HTML
WEB2MD   "https://example.com/docs"             → webpage → markdown


### Formati supportati

## AI Providers
| Comando | Sintassi | Descrizione |
|---|---|---|
| `PROVIDERS` | `PROVIDERS` | Menu interattivo configurazione provider LLM |
| `PROVIDERS LIST` | `PROVIDERS LIST` | Elenca tutti i provider configurati con modello e stato API key |
| `PROVIDERS SETKEY` | `PROVIDERS SETKEY <name> <key>` | Imposta API key per un provider |
| `PROVIDERS WIZARD` | `PROVIDERS WIZARD` | Wizard guidato per nuovo provider |
| `PROVIDERS LOAD` | `PROVIDERS LOAD` | Carica provider da `config/providers.json` |
| `PROVIDERS SAVE` | `PROVIDERS SAVE` | Salva provider su `config/providers.json` |

### Provider pre-configurati (17, senza API key)
| Provider | Base URL |
|---|---|
| `openrouter-default` | `https://openrouter.ai/api/v1` |
| `deepseek-default` | `https://api.deepseek.com/v1` |
| `groq-default` | `https://api.groq.com/openai/v1` |
| `mistral-default` | `https://api.mistral.ai/v1` |
| `qwen-default` | `https://dashscope-intl.aliyuncs.com/compatible-mode/v1` |
| `cerebras-default` | `https://api.cerebras.ai/v1` |
| `nvidia-default` | `https://integrate.api.nvidia.com/v1` |
| `together-default` | `https://api.together.xyz/v1` |
| `fireworks-default` | `https://api.fireworks.ai/inference/v1` |
| `mimo-default` | `https://api.xiaomimimo.com/v1` |
| `metis-default` | `https://api.metisrouter.com/v1` |
| `ollama-cloud-default` | `https://ollama.com/v1` |
| `perplexity-default` | `https://api.perplexity.ai` |
| `xAI-default` | `https://api.x.ai/v1` |

### Configurazione rapida

PROVIDERS SETKEY openrouter-default sk-or-v1-...   ← aggiungi chiave
PROVIDERS SETKEY deepseek-default sk-...            ← aggiungi chiave
PROVIDERS LIST                                      ← verifica


## Download Modelli (comandi REPL-only — Fase 4 del Native-LLM-Provider-Plan)
| Comando | Sintassi | Descrizione |
|---|---|---|
| `AIPATH` | `AIPATH` | Mostra i path di ricerca modelli (`AIPATH=...` per impostarli, `;`-separati) |
| `AIPATH=...` | `AIPATH=~/ai/gguf;~/ai/Models;` | Imposta la lista path di ricerca, poi ristampa il valore. Default: `shared/samples/ai/models/<tipo>/` |
| `LLM` | `LLM [GGUF\|MLX\|OLLAMA]` | **Elenca i modelli** trovati in tutti i percorsi `AIPATH` (tipo rilevato dal contenuto della cartella, mai dichiarato a mano), ordinati per tipo/nome: `[GGUF]  nome  /path`. Filtro opzionale per tipo; `LLM --help`/`-h` per l'aiuto. Se `AIPATH` non è impostato cerca i default `shared/samples/ai/models/gguf\|mlx/` + `~/.ollama/models` (sempre controllato). **Ripristinato 2026-10-06** (era l'unico comando della vecchia GM-Basic assente); logica in `REPL/ReplEngine.cs` (`HandleLlmAsync`) → `ModelScanner.ScanAll` |
| `HF` | `HF <org/repo> [opzioni]` | Scarica file GGUF da un repo HuggingFace (`--file` richiesto se più `.gguf`) |
| `MLX` | `MLX <org/repo> [opzioni]` | Scarica l'intero repo MLX (stessa semantica di `rapid-mlx pull`) |

### Opzioni comuni (`--help`/`-h` per l'elenco completo)
| Opzione | Default | Descrizione |
|---|---|---|
| `--file <nome\|pattern>` | — | Solo `HF`: file GGUF da scaricare (supporta wildcard `*`, multi-file) |
| `--dest <path>` | primo path di `AIPATH` | Cartella di destinazione |
| `--stall-timeout <min>` | 5 | Minuti senza crescita del file prima di kill/riavvio (resume `-C -`) |
| `--stall-retries <n>` | 1 | Riattacchi per stallo prima di arrendersi |
| `--workers <n>` | 8 | Download paralleli (repo multi-file) |
| `--no-xet` | off | no-op (curl via HTTP classico, mai Xet) |
| `--no-token` | off | Non invia il token HF |
| `--no-curl` | — | Non supportato nella porta nativa: errore esplicito |
| `--quiet` | off | Solo esito finale, senza progresso |

AIPATH                                     ' mostra: AIPATH= (non impostato; default shared/samples/ai/models/<tipo>/)
AIPATH=~/ai/gguf;~/ai/Models;              ' imposta i path di ricerca
HF bartowski/Qwen2.5-Coder-32B-Instruct-GGUF --file Qwen2.5-Coder-32B-Instruct-Q4_K_M.gguf
MLX mlx-community/Qwen3-Coder-30B-A3B-Instruct-8bit --dest ~/ai/models


## Runtime componenti (comando `SETUP`) — librerie native, torch, Chromium in cache
| Comando | Sintassi | Descrizione |
|---|---|---|
| `SETUP LIBS` | `SETUP LIBS [--force] [--quiet]` | **Tutte** le librerie native di `~/.gmbasic/libs/<rid>/` (SDL2+extras, SDL3, freetype, harfbuzz, pdf_oxide, vision, crypto/ssl, **tesseract**) → download on-demand da GMBasicModules (l'OCR è incluso, niente SETUP OCR separato) |
| `SETUP TORCH` | `SETUP TORCH [--force] [--quiet]` | Librerie runtime ML (`libgmb_torch` + libtorch) → `~/.gmbasic/libs/ml/<rid>/`. Dopo, `TENSOR.*`/`TRAIN.*` funzionano senza configurazione |
| `SETUP BROWSER` | `SETUP BROWSER [--force] [--quiet]` | Chromium (Chrome for Testing) per il plugin browser → `~/.gmbasic/libs/chromium/<rid>/` |
| `SETUP ALL` | `SETUP ALL [--yes]` | Tutti i componenti sopra (chiede conferma con l'elenco) |
| `SETUP STATUS` | `SETUP STATUS` | RID corrente + stato cache di TUTTI i componenti |
| `SETUP RM <comp>` | `SETUP RM <LIBS\|TORCH\|BROWSER\|ALL> [--force]` | Elimina la cache del componente (o tutti), con conferma |
| `SETUP HELP` | `SETUP HELP` | Elenco componenti e cosa scarica ognuno |

## Bundle librerie di supporto (comando `LIBS`)
| Comando | Sintassi | Descrizione |
|---|---|---|
| `LIBS UPDATE` | `LIBS UPDATE [<rid>\|ALL]` | Rigenera i `.zip` per RID (tool **GMBasic** `tools/gmb_libs_update.bas`: `COMPRESSION.ZIP` + `CRYPTOGRAPHY.STRHASHFILE`) e scrive `~/.gmbasic/libs-manifest.json` (version/data, archiveUrl, sha256, size) |
| `LIBS CHECK` | `LIBS CHECK` | Confronta lo `sha256` del manifest con i bundle correnti; **exit≠0** se stale |
| `LIBS PUBLISH` | `LIBS PUBLISH [<date>]` | Crea/aggiorna la release `gmb-libs-<date>` su GMBasicModules e carica gli asset (richiede `gh` autenticato) |
| `LIBS HELP` | `LIBS HELP` | Elenco dei sottocomandi |

## RAG / Graph (comandi REPL — costruzione RAG da wiki + grafi Obsidian)

### Comandi RAG
| Comando | Sintassi | Descrizione |
|---|---|---|
| `RAG BUILD` | `RAG BUILD <wikiDir> [--rag dir] [--name db] [--chunk N] [--overlap N] [--dry-run] [--force] [--prune]` | Costruisce l'indice RAG dell'intero folder wiki (default chunk 2048/overlap 256; db in `<ragDir>/<name>.db`; nome default = cartella wiki). **Richiede un provider embedding reale** (llamacpp/mlx/ollama) — rifiuta il fixture |
| `RAG UPDATE` | `RAG UPDATE <wikiDir> [opzioni]` | Aggiornamento incrementale: indicizza solo file nuovi/modificati (hash SHA256), skip invariati. Alias: `RAG REFRESH` (= UPDATE con `--prune`) |
| `RAG STATUS` | `RAG STATUS [--rag dir] [--name db]` | Stato del db: presente?, provider embedding attivo (+ marca `NON USARE per RAG` se fixture), file indicizzati, ultimo aggiornamento |
| `RAG QUERY` | `RAG QUERY "<query>" [--rag dir] [--name db] [--topk N]` | Scorciatoia per `AI.SEARCH` sul db appena costruito (per validare senza scrivere BASIC) |

### Comandi Graph
| Comando | Sintassi | Descrizione |
|---|---|---|
| `GRAPH BUILD` | `GRAPH BUILD <wikiDir> [--graphs dir] [--name nome] [--pagerank-damping D] [--pagerank-iterations N] [--dry-run] [--force] [--prune] [--no-html]` | Costruisce il grafo stile Obsidian: nodi = file `.md`, edge = wikilink `[[path\|alias]]` risolti (fallback basename case-insensitive), dangling contati. Output: `<name>.graphml` (yEd/Gephi), `<name>.mermaid` (`graph TD`), `<name>.json`, `<name>.stats.json` (nodi/edge/dangling/top-hub/PageRank/community/degree/betweenness) e — salvo `--no-html` — `<name>.html` (viewer interattivo autonomo). Scala: AiFinanceWiki intero (28.087 file) in ~50s |
| `GRAPH UPDATE` | `GRAPH UPDATE <wikiDir> [opzioni]` | Incrementale: solo nodi/edge nuovi/modificati. Alias: `GRAPH REFRESH` (= UPDATE con `--prune`) |
| `GRAPH STATUS` | `GRAPH STATUS [--graphs dir] [--name nome]` | Conteggi + stato resume (file processati/ultimo aggiornamento) |
| `GRAPH VIEW` | `GRAPH VIEW [--graphs dir] [--name nome] [--no-open]` | Apre `graph.html` nel browser (viewer autonomo: community/degree, zoom/pan, filtri, ricerca) |
| `GRAPH WATCH` | `GRAPH WATCH <wikiDir> [--graphs dir] [--name nome]` | Watchdog: auto-rebuild del grafo a ogni modifica/creazione/cancellazione `.md` (debounce ~1s). `GRAPH UNWATCH <wikiDir>` lo ferma |
| `GRAPH EXPORT OBSIDIAN` | `GRAPH EXPORT OBSIDIAN [--graphs dir] [--name nome] [--out dir]` | Esporta un vault Obsidian navigabile: `index.md` + nota per community con wikilink |
| `GRAPH ENRICH SEMANTIC` | `GRAPH ENRICH SEMANTIC <wikiDir> [--graphs dir] [--name nome] [--max N] [--batch N]` | L'LLM attivo (`%CHAT`) legge il contenuto delle note e deduce relazioni implicite (edge tipo `semantic`). Richiede un provider chat attivo; senza → errore esplicito |

### Esempi

RAG BUILD /Users/gaetano/Documents/AiFinanceWiki --dry-run      ' stima prima di partire
RAG BUILD /Users/gaetano/Documents/AiFinanceWiki               ' build completo (27.872 file)
RAG UPDATE /Users/gaetano/Documents/AiFinanceWiki              ' aggiornamento quotidiano
RAG QUERY "risk parity" --name AiFinanceWiki --topk 5           ' verifica
GRAPH BUILD /Users/gaetano/Documents/AiFinanceWiki --name AiFinanceWiki -H   ' 28k nodi, salta l'html
GRAPH UPDATE /Users/gaetano/Documents/AiFinanceWiki --name AiFinanceWiki
GRAPH STATUS --name AiFinanceWiki
GRAPH VIEW --name AiFinanceWiki                               ' apre il viewer interattivo
GRAPH WATCH /Users/gaetano/Documents/AiFinanceWiki --name AiFinanceWiki   ' rebuild automatico
GRAPH EXPORT OBSIDIAN --name AiFinanceWiki --out /tmp/vault   ' vault Obsidian navigabile
GRAPH ENRICH SEMANTIC /Users/gaetano/Documents/AiFinanceWiki --max 100   ' LLM deduce relazioni


### Parametri configurabili (RAG e GRAPH)
| Parametro | Default | Flag | Alias | Chiave `ai.json` |
|---|---|---|---|---|
| Chunking `MaxChars` | 2048 | `--chunk N` | `-c N` | `ragChunkMaxChars` |
| Chunking `Overlap` | 256 | `--overlap N` | `-o N` | `ragChunkOverlap` |
| Collection RAG | `wiki` | `--collection c` | `-C c` | `ragCollection` |
| DB nome | nome cartella wiki | `--name db` | `-n db` | — |
| Cartella RAG | `<root>/rag/` | `--rag dir` | `-r dir` | `ragDefaultDir` |
| Cartella graphs | `<root>/graphs/` | `--graphs dir` | `-g dir` | `graphsDefaultDir` |
| PageRank damping | 0.85 | `--pagerank-damping D` | `-d D` | `graphPageRankDamping` |
| PageRank iterazioni | 100 | `--pagerank-iterations N` | `-i N` | `graphPageRankIterations` |
| top-k query | 5 | `--topk N` | `-t N` | — |
| dry-run | — | `--dry-run` | `-D` | — |
| force | — | `--force` | `-f` | — |
| prune | — | `--prune` | `-p` | — |
| skip viewer html | — | `--no-html` | `-H` | — |
| note LLM (enrich) | — | `--max N` | — | — |
| batch LLM (enrich) | 6 | `--batch N` | — | — |
| Dimensione embedding | dal provider attivo | — | — | — |
| Estrazione binari (PDF/docx) | `DocumentConverter` | — | — | — |

RAG BUILD /wiki -C finance -c 1024 -o 128        ' sovrascrive ragCollection/ragChunk*
GRAPH BUILD /wiki -d 0.5 -i 50                   ' sovrascrive graphPageRank*


## Brain (comandi REPL — Second Brain: USE/INSTALL/EXPORT/SIGN)
| Comando | Sintassi | Descrizione |
|---|---|---|
| `BRAIN USE` | `BRAIN USE <name> [, <name>...]` | Collega uno o più cervelli GIÀ disponibili su disco (verifica `available`), multi-attach. Errore esplicito se il cervello non esiste |
| `FIXTURES` | `FIXTURES LIST` · `FIXTURES ENSURE` · `FIXTURES PULL <nome…>` | Fixture del **compilatore** (dati di input di sample e test): vivono SEMPRE in `shared/samples/Fixtures/<nome>/` (manifest `shared/settings/fixtures-manifest.json`). `LIST` = presente/mancante, `ENSURE` = scarica solo le mancanti, `PULL` = scarica esplicitamente. Download con resume + sha256; nessun symlink, nessun disco esterno |
| `BRAIN DETACH` | `BRAIN DETACH <name>` | Scollega (i file restano su disco) |
| `BRAIN LIST` | `BRAIN LIST` | Elenca i brain registrati (`*` = collegato) |
| `BRAIN INFO` | `BRAIN INFO <name>` | Info: mode, available, checksum, attached, source, docs, probe |
| `BRAIN INSTALL` | `BRAIN INSTALL <name> <zip-or-url> [--path dir] [--activate]` | Importa un cervello GIÀ COSTRUITO da zip `.gmbrain` (remoto URL o locale). Scarica/estrai/valida (`brain.manifest.json` + checksum) e installa in `brains/<name>/` — nella root del progetto (o `--path`) con rollback su errore |
| `BRAIN EXPORT` | `BRAIN EXPORT <name> [--out file.zip]` | Genera lo zip `.gmbrain` di un brain esistente (per spostarlo fra macchine) |
| `BRAIN SIGN` | `BRAIN SIGN <name> <dir> [--out file.zip]` | Firma e marca come valido un cervello FORNITO DALL'UTENTE (folder non generata da GMBasic) con **probe minimi**: graph (nodi da .md/wikilink o grafo pronto) e/o vectordb (.db vettoriale interrogabile). Nessun embedding richiesto per la firma. Se nessun probe passa → errore esplicito con report |
| `BRAIN STATS` | `BRAIN STATS <name> [--json]` | Statistiche del cervello: modalità/available/attached, documenti+chunk+dimensione del db RAG, nodi/edge/dangling/community del grafo, checksum e root. **V1 del brain toolchain** (2026-09-16) |
| `BRAIN VERIFY` | `BRAIN VERIFY <name> [--json]` | **Integrità** del cervello: manifest+checksum dei file (assente = ok per i cervelli costruiti localmente), db RAG apribile (`PRAGMA integrity_check`, documenti/chunk/embedding/FTS5), grafo deserializzabile + rapporto dangling (< 50%). Exit code ≠ 0 se un controllo fallisce (usabile in CI) |
| `BRAIN CREATE` | `BRAIN CREATE <name> --from <dir> [--mode rag\|graph\|both] [--register] [--activate] [--dry-run]` | Costruisce un cervello da una cartella riusando la pipeline ufficiale (`RagIndexService` / `GraphStore`+`GraphExporter`) — **nessun builder nuovo**. `--dry-run` non scrive nulla. `--register` censisce il cervello, `--activate` lo collega |
| `BRAIN OPTIMIZE` | `BRAIN OPTIMIZE <name> [--dry-run] [--reindex-fts]` | Manutenzione: RAG = `VACUUM` + `PRAGMA optimize` + FTS5 `optimize` (o `rebuild` con `--reindex-fts`); GRAPH = ricalcolo degli export (stats/mermaid/graphml/html con PageRank/community). Stampa la dimensione prima/dopo |
| `BRAIN BACKUP` | `BRAIN BACKUP <name> [--out file.zip]` | Zip `.gmbrain` del cervello (riusa `BrainPackager`); senza `--out` il file va in `brains-exports/<name>-<timestamp>.gmbrain.zip` |
| `BRAIN REGISTER` | `BRAIN REGISTER <name> [--mode rag\|graph\|both] [--source dir] [--activate]` | Censisce nel registry un cervello i cui artefatti **esistono già** (`gmb rag build`/`gmb graph build`): senza registrazione il cervello è usabile (`BRAIN USE`) ma non compare in `LIST`/`INFO`/`STATS`/`VERIFY` |
| `BRAIN CONVERT` | `BRAIN CONVERT <name> --to rag\|graph [--out <nuovo>\|--in-place] [--min-edges N] [--dry-run]` | Deriva l'artefatto mancante: rag→graph (nodi dai `source_id`, edge dai wikilink `[[…]]` dei chunk; sotto `--min-edges` è **errore esplicito**), graph→rag (un documento per nodo; richiede un embedder reale) |
| `BRAIN DIFF` | `BRAIN DIFF <a> <b> [--json]` | Differenze fra due cervelli: documenti RAG (per `source_id`) e nodi del grafo, con campioni |
| `BRAIN DOCTOR` | `BRAIN DOCTOR <name>` | `VERIFY` + fix **sicuri** (PRAGMA optimize, FTS5 rebuild, VACUUM, prune dei dangling) + riverifica; su un cervello sano non tocca nulla |
| `BRAIN BUILDVAULT` | `BRAIN BUILDVAULT [--out dir]` | Rigenera il **brain di auto-conoscenza** dalla StdLib (pagine per namespace + indice RAG + registrazione); attiva il provider embedding come `RAG BUILD` |
| `BRAIN SCOPE` | `BRAIN SCOPE [INHERIT\|PRIVATE] [--without a,b] [--reset]` | Livello **SESSIONE** della catena di scope dei cervelli: `INHERIT` (default) vede anche i brain collegati, `PRIVATE` vede solo i brain dichiarati con `BRAIN USE … --scope session`; `--without` sottrae nomi/alias. Mostra la catena + set effettivo |
| `BRAIN RESOLVE` | `BRAIN RESOLVE <name\|alias> [--explain] [--private] [--without a,b]` | Risolve un nome/alias nella catena di scope (`CALL → TASK → AGENT → SESSION → PROCESS`); `--explain` mostra i livelli attraversati e il set effettivo. Exit ≠ 0 se non risolto |

### Esempi

' costruisci un brain (REPL/CLI — mai on-demand in INSTALL)
RAG BUILD /Users/gaetano/Documents/AiFinanceWiki --name trading
GRAPH BUILD /Users/gaetano/Documents/AiFinanceWiki --name trading
' collega (verifica file su disco)
BRAIN USE trading
BRAIN USE airline, trading, legal        ' multi-attach
' importa uno zip già costruito (da un'altra macchina o da un repo)
BRAIN INSTALL airline https://brains.example.com/airline.gmbrain.zip --activate
BRAIN INSTALL legal /tmp/legal.gmbrain.zip --path /Users/gaetano/my-brains
' esporta un brain per spostarlo
BRAIN EXPORT trading --out /tmp/trading.gmbrain.zip
' firma un cervello fornito dall'utente (folder non-GMBasic)
BRAIN SIGN mywiki /tmp/mywiki --out /tmp/mywiki.gmbrain.zip


### Costruire e testare un brain — procedura end-to-end (2026-09-09)

' 1. PREREQUISITO RAG: serve un embedding REALE attivo (il fixture NON è adatto).
'    Esempi già registrati in config/providers.json: llamacpp-emb (bge-small-en-v1.5,
'    384-dim, GGUF) e mlx-emb (Llama-3.2-1B, latent). Se l'attivo è fixture, il build
'    lo scarta e attiva un non-fixture; se non c'è nessun embedding reale → errore esplicito.
'    (Verifica: gmb rag status → "provider embedding attivo: <alias>")

' 2. COSTRUISCI il brain (REPL/CLI — mai on-demand in INSTALL). Il vault sorgente va dato
'    come PATH ASSOLUTO al vault di DOMINIO (es. AiAirlineWiki/AiFinanceWiki), NON al vault
'    di sviluppo AiWiki. Artefatti: rag/<name>.db + graphs/<name>.*
gmb rag build   /Users/gaetano/Documents/AiAirlineWiki --name airline
gmb graph build /Users/gaetano/Documents/AiAirlineWiki --name airline
'    Esito atteso RAG: "[OK] indicizzati N ... skipped M". Esito atteso GRAPH:
'    "[OK] nodi N, edge M ..." (stats in graphs/<name>.stats.json).

' 3. VERIFICA la disponibilità e COLLEGA
gmb brain info airline      ' → mode: both, available: [rag, graph]


## Eval Harness AI — `AIBENCH` (2026-09-07, M6/C3; rinominato 2026-11-01)
| Comando | Sintassi | Descrizione |
|---|---|---|
| `AIBENCH` | `AIBENCH [RUN] [path]` | Esegue l'**Eval Harness AI** (`GMBasicSharp/common/AI/Eval/EvalHarness.cs`): suite metriche `llm.tokps`, `rag.ndcg@2`, `team.acc`, `credit.blocked`. Se `path` è dato scrive `eval_report.json`. **Fixture deterministico** se nessun provider LLM attivo (mai rete implicita); col provider attivo usa il modello reale. Stampa il riepilogo per suite |

AIBENCH RUN work/eval_report.json
AIBENCH                      ' solo sommario, fixture se nessun LLM


## Notebook — progetto autonomo, NON un comando della REPL

cmake -S Notebook -B Notebook/build && cmake --build Notebook/build --target gmbnotebook
Notebook/build/gmbnotebook --port 8030


## Package Manager
| Comando | Sintassi | Descrizione |
|---|---|---|
| `PACKAGE` | `PACKAGE` | Menu package manager |
| `PACKAGE INSTALL` | `PACKAGE INSTALL "<name>" [@ "<ver>"] [FROM "<url>"] [LOCAL \| TO "<dir>"]` | Installa nella **cache**; `LOCAL` → `<dir dell'eseguibile>/packages`, `TO "<dir>"` → `<dir>/packages`. Se la stessa versione è già nell'altra cartella viene **spostata** (niente download). `FROM` = `--registry` |
| `PACKAGE RUN` | `PACKAGE RUN <name>` | Esegue un pacchetto vm-chunk come programma (il suo `SUB Main`), senza un main che lo richieda — §1 vm-chunk standalone |
| `PACKAGE UPDATE` | `PACKAGE UPDATE [dir] [--check] [--yes] [FROM "<url>" \| --registry <url>] [--cache <dir>]` | Aggiorna i pacchetti del progetto |
| `PACKAGE RESTORE` | `PACKAGE RESTORE [dir] [--locked] [FROM "<url>" \| --registry <url>] [--cache <dir>]` | Reinstalla dal lock |
| `PACKAGE <sub>` | `PACKAGE INFO\|LIST\|REMOVE\|SEARCH\|TREE\|OUTDATED\|VENDOR\|WHY\|PRUNE\|DEDUPE\|AUDIT\|SBOM\|DEPRECATE\|UNDEPRECATE\|BUILD\|OWNERS\|YANK\|TAG\|TAGS\|SYNC …` | Sottocomandi PM inoltrati alla **stessa** implementazione della CLI (`CliPackages.Pkg`); accettano `--registry`/`--cache`/`--token`/`--lock` |
| `RESTORE` | `RESTORE [dir] [--locked] [--frozen] [--offline] [--production] [--verify] [--require-signed] [FROM <url>]` | Installa le dipendenze dai `REQUIRE` (+ `gmb-package.json`). `--frozen` (CI) fallisce se il lock divergerebbe; `--offline` solo cache/vendor; `--production` esclude dev; `--verify` integrità vs lock; `--require-signed` firma obbligatoria |
| `UPDATE` | `UPDATE [dir] [--check] [--yes] [--production] [FROM <url>]` | Aggiorna **tutti** i pacchetti del progetto all'ultima versione consentita; riscrive il lock |
| `TRUST` | `TRUST ADD\|REMOVE\|LIST\|FINGERPRINT …` | Keyring locale delle chiavi fidate (`~/.gmbasic/trusted-keys.json`) |
| `ROLLBACK` | `ROLLBACK [dir] [--list] [--to <timestamp>]` | Ripristina uno storico di `gmb-package.lock` (`.gmbasic/lock-history/`) e re-installa |
| `BUMP` | `BUMP <dir> <major\|minor\|patch\|prerelease\|X.Y.Z>` | Aggiorna la versione nel manifest |
| `ADD` | `ADD <name[@ver]> [--dev] [--dir dir]` | Aggiunge/aggiorna una dipendenza in `gmb-package.json` (`dependencies`, o `devDependencies` con `--dev`) |
| `REMOVE` | `REMOVE <name> [--dir dir]` | Rimuove una dipendenza da `gmb-package.json` (`dependencies` + `devDependencies`) |
| `CACHE` | `CACHE list\|verify\|dedup\|gc\|clean [--cache dir]` | Gestione cache; `dedup`/`gc` = content-addressable (blob+hardlink) |
| `gmb register` | ~~`gmb register <username>`~~ | **RIMOSSO**: gli utenti li crea SOLO l'admin (`gmb admin user create`). Il comando mostra un messaggio che lo spiega. |
| `gmb pkg owners` | `gmb pkg owners <add\|remove> <package> <user> [--registry url]` | Gestisce i collaboratori di un pacchetto — solo l'owner |
| `gmb pkg yank` | `gmb pkg yank <package> <version> [--un] [--reason r] [--registry url]` | Yank (ritira) una versione, `--un` per unyank — download risponde 410 |
| `gmb pkg sync` | `gmb pkg sync <package> --tag vX [--registry url]` | Sincronizza il tag di un pacchetto contributor (solo owner/collab) |
| `gmb publish` | `gmb publish <file.gmbpkg> [--registry url] [--token t]` | Pubblica un `.gmbpkg` (ufficiale). Prima versione = claim del nome; versione esistente → 409 |
| `gmb publish --repo` | `gmb publish --repo <git-url\|path> <name@tag> [--registry url]` | Pubblica da repo git (contributor): verifica locale 5 passi → registra sul registry proxy |
| `gmb admin user` | `gmb admin user create\|setkey\|status\|list ...` | **Gestione utenti (solo admin)**: crea (token 1 volta, nasce blocked), setkey (chiave Ed25519), status active/blocked, list |
| `gmb admin reserve` | `gmb admin reserve <name> [--registry url] [--token t]` | Riserva un nome (blocca il claim automatico) |
| `gmb admin assign` | `gmb admin assign <name> <user> [--registry url] [--token t]` | Assegna l'owner di un nome (anche orfano) |
| `gmb admin revoke` | `gmb admin revoke <name> [--keep-reserved] [--registry url] [--token t]` | Revoca l'ownership (keep-reserved = torna riservato) |
| `gmb admin password` | `gmb admin password <newToken> [--registry url] [--token t]` | Cambia il token admin (il vecchio smette subito) |

### Esecuzione comandi di sistema (BASIC) — `SYSTEM.SHELL`
| Sintassi | Tipo | Descrizione |
|---|---|---|
| `SET SHELL ON` / `SET SHELL OFF` | comando | **Opt-in**: abilita/disabilita l'esecuzione di comandi di sistema (default **OFF**) |
| `SHELL "cmd"` | comando | Esegue `cmd` (via `/bin/sh -c` o `cmd /c`), scarta l'output |
| `out$ = SYSTEM.SHELLOUT("cmd")` | funzione | Esegue e ritorna stdout+stderr |
| `code = SYSTEM.SHELLCODE("cmd")` | funzione | Esegue e ritorna l'exit code |

## Generali
| Comando | Descrizione |
|---|---|
| `QUIT` / `EXIT` | Esci dal REPL |
| `CLS` | Pulisci lo schermo |
| `HELP` | Mostra questa guida |

## CLI Flags (all'avvio)

gmb <file.bas> [flags]
  -m, --managed     Backend managed: DLL + apphost stub (richiede .NET installato)
  -n, --native      Backend nativo: singolo binario NativeAOT (nessun runtime .NET a carico)
                    (default senza flag = P-Code con PCodeVM interna, in-process)
  -repl             Rimani nel REPL dopo esecuzione
  -pcode            Solo P-Code (PCodeVM interna: scrive .prg + esegue)
  -full             ⚠️ legacy (D3): runtime .NET self-contained, NON più lo standard (IL = AOT)
  -bsp <dir>        Pre-compila pagine .bsp
  --run <cmd>       Esegue un comando/riga all'avvio e termina (non-interattivo) —
                    es. `gmb --run "LLM"`, `gmb --run "AST ALL"`, `gmb --run "RUN prog.bas"`.
                    Ripristinato 2026-10-06 (era nel vecchio Executor.Startup: "esegui un comando
                    diretto"): la REPL è così pilotabile da script/CI senza TTY interattivo.
  --no-optimize     Salta ILOptim
  --dump-il         Genera disassembly IL
  -gpm:always       Modalità GPM (C99): auto|always|linear — default always
                    (always = GPM sempre, rete di sicurezza; linear = percorso veloce solo se sicuro)


## Appendice storica — Rapid-MLX (identità dei repo)

### Brain-Toolchain-Guide

*Fonte: `Brain-Toolchain-Guide`*


## 0. Concetti in 30 secondi
| Concetto | Cos'è |
|---|---|
| **cervello (brain)** | un nome che raggruppa artefatti: `rag/<nome>.db` (indice vettoriale) e/o `graphs/<nome>.json` (+ export) |
| **mode** | `rag`, `graph` o `both` — `BRAIN STATS` mostra `available` |
| **registrare** | censisce il cervello in `config/brains.json` (senza, è usabile con `BRAIN USE` ma non compare in `LIST/INFO/STATS`) |
| **collegare (attach)** | `BRAIN USE <nome>`: da lì in poi `AI.SEARCHMULTI`/`BRAINS.QueryMulti` lo interrogano |
| **scope** | la catena `CALL → TASK → AGENT → SESSION → PROCESS` che decide *quali* cervelli vede un agente/una sessione (`BRAIN SCOPE`, `BRAIN RESOLVE`) |

## 1. Ciclo di vita

gmb brain create trading --from /Users/gaetano/Documents/AiFinanceWiki/01_Strategies --mode both --register
gmb brain stats  trading                     # documenti/chunk, nodi/edge, dimensione, manifest
gmb brain verify trading                     # INTEGRITÀ (exit 1 se un controllo è KO)
gmb brain verify trading --golden golden.jsonl --topk 5 --min-p1 0.8   # QUALITÀ del retrieval
gmb brain use    trading                     # collega (multi: gmb brain use airline, poi trading)
gmb brain optimize trading --dry-run         # cosa farebbe (VACUUM/optimize/FTS5, re-export grafo)
gmb brain backup trading                     # zip .gmbrain in brains-exports/<nome>-<timestamp>
gmb brain rename trading trading-v2 --dry-run


## 2. Manutenzione e qualità del contenuto
| Comando | Cosa fa | Note |
|---|---|---|
| `BRAIN DEDUPE <nome> [--apply]` | duplicati: chunk con testo identico (normalizzato) e chunk-crumbs (< 40 char); edge duplicati e self-loop del grafo | **report** di default; `--apply` rimuove (chunk + embedding + righe FTS) |
| `BRAIN OPTIMIZE <nome> [--reindex-fts]` | RAG: `PRAGMA optimize` + FTS5 `optimize`/`rebuild` + `VACUUM`; GRAPH: ricalcolo export (PageRank/community) | stampa la dimensione prima/dopo |
| `BRAIN DOCTOR <nome>` | `VERIFY` + fix **sicuri** (FTS5 rebuild, VACUUM, prune dei dangling) + riverifica | su un cervello sano non tocca nulla |
| `BRAIN DIFF <a> <b> [--json]` | differenze RAG (documenti) e GRAPH (nodi) con campioni | utile prima di un merge |
| `BRAIN BUILDVAULT [--out dir]` | rigenera il **brain di auto-conoscenza** (pagine per namespace dalla StdLib + indice RAG) | richiede un provider embedding reale; è il percorso di `AI.BRAINVAULTBUILD` da CLI |

## 3. Composizione
| Comando | Cosa fa |
|---|---|
| `BRAIN EXTRACT <nome> --out <nuovo> [--source <substr>] [--node <substr>] [--hops N]` | copia un **sottoinsieme**: RAG = documenti il cui `source_id` contiene la sottostringa (chunk+embedding copiati, **nessun re-embedding**); GRAPH = sottografo dei nodi che matchano + vicini a N hop |
| `BRAIN MERGE <a> <b> [<c>…] --out <nuovo> [--on-conflict skip\|replace]` | unisce: RAG con dedup per `source_id` (provenance `originBrain`), GRAPH con dedup di nodi/edge + export ricalcolati. Precondizione: **stessa dimensione embedding** |
| `BRAIN CONVERT <nome> --to rag\|graph [--out <nuovo>\|--in-place] [--min-edges N]` | rag→graph: nodi dai `source_id`, edge dai wikilink `[[…]]` dei chunk; graph→rag: un documento per nodo (richiede embedder reale) |

## 4. Scope e diagnostica
| Comando | Cosa fa |
|---|---|
| `BRAIN SCOPE [INHERIT\|PRIVATE] [--without a,b] [--reset]` | livello **SESSIONE**: `PRIVATE` vede solo i cervelli dichiarati con `BRAIN USE … --scope session` |
| `BRAIN USE <nome> [AS <alias>] [--scope session]` | `--scope session` dichiara nel livello sessione (registry globale **invariato**); `AS <alias>` dà un nome locale |
| `BRAIN RESOLVE <nome\|alias> [--explain] [--private] [--without a,b]` | risolve un nome nella catena e dice **quale livello** l'ha risolto (`--explain` mostra catena + set effettivo); exit 1 se non risolto |
| `BRAIN LIST` / `INFO` / `STATS` / `VERIFY` | `*` = collegato; `INFO`/`STATS` = mode/available/docs; `VERIFY` = integrità |

## 5. Trappole (imparate sul campo)

## 6. Esempio end-to-end (verificato)

## 7. Vedi anche

### GMBasicIDE-Guida-Utente

*Fonte: `GMBasicIDE-Guida-Utente`*


## 1. Progetti

### Nuovo progetto GMBasic

### Aprire un progetto

### Proprietà progetto
| Campo | Significato |
|---|---|
| `Name` | nome visualizzato del progetto |
| `Entry` | file `.bas` di ingresso, relativo alla cartella |
| `Backend` | backend GMBasic (`c99native`, `il`, ...) |

### File e moduli

## 2. Designer VB6

## 3. Properties

DIM p AS Panel = NEW Panel()
CONTROL ADD p AT 2, 2 SIZE 40, 12
p.FontName = "Arial"
p.FontSize = 10
p.Appearance = 1
p.StartUpPosition = 2


## 4. Code editor

## 5. Debug

## 6. Tool

## 7. Asset e attribuzione

## 8. Limiti noti

### Notebook-User-Guide

*Fonte: `Notebook-User-Guide`*


## Cos'è un notebook GMBASIC

## Come si avvia

### Nel terminale (REPL)

NOTEBOOK START            ' avvia il server su http://127.0.0.1:8030/
NOTEBOOK START 9000       ' su una porta diversa
NOTEBOOK STATUS           ' mostra stato e porta
NOTEBOOK STOP             ' ferma il server


### Dentro l'IDE (GMBasicIde)

## Celle di codice

PRINT "ciao mondo"
x = 42
y = x * 2
PRINT y


### Scorciatoie e pulsanti
| Comando | Effetto |
|---|---|
| **Ctrl+Enter** | Esegue la cella corrente |
| **Shift+Enter** | Va a capo dentro la cella (multiriga) |
| **▶ Run all** | Riesegue **tutte** le celle da zero (stato pulito), in ordine dall'alto in basso |
| **+ Cella** | Aggiunge una cella vuota in fondo |
| **Reset** | Azzera lo stato (variabili, cache, database del notebook) |

## Le celle speciali (magic)

### `%md` — testo / documentazione

%md # Introduzione
Questo notebook analizza i dati delle vendite.


### `%sql` — interrogare un database

%sql CREATE TABLE vendite (prodotto TEXT, importo REAL)
%sql INSERT INTO vendite VALUES ('tavolo', 250), ('sedia', 90)
%sql SELECT * FROM vendite
%sql EXPLAIN QUERY PLAN SELECT * FROM vendite WHERE importo > 100


### `%ai` — chiedere a un modello

%ai spiega in tre righe cos'è un indice di database


### `%vdb` — ricerca semantica nel notebook

%vdb add doc1 = il motore MLX gira in C puro su Apple Silicon
%vdb add doc2 = la ricetta della torta al cioccolato richiede uova e farina
%vdb search motore 3          ' cerca (fino a 3 risultati)
%vdb count                    ' quanti documenti ci sono
%vdb clear                    ' svuota l'archivio


### `%chart` — grafici

## Salvare, riprendere, esportare

### Salvare e ricaricare (`.gmnb`)

NOTEBOOK SAVE work/analisi.gmnb        ' dal REPL
NOTEBOOK LOAD work/analisi.gmnb


### Autosave

NOTEBOOK AUTOSAVE work/analisi.gmnb    ' ogni cella eseguita salva subito
NOTEBOOK AUTOSAVE OFF                  ' smette


### Esportare come programma BASIC (`.bas`)

NOTEBOOK EXPORT work/analisi.bas


### Esportare come documento (`.html` / `.pdf`)

NOTEBOOK EXPORT work/analisi.html    ' HTML print-ready (font monospace, page-break per cella)
NOTEBOOK EXPORT work/analisi.pdf     ' PDF REALE via Chromium (richiede ~/.gmbasic/libs/chromium)


## Esempio completo

%md # Calcolo della media
%sql CREATE TABLE note (nome TEXT, voto REAL)
%sql INSERT INTO note VALUES ('italiano', 8), ('matematica', 7.5), ('scienze', 9)

total = 0
FOR i = 1 TO 100
   total = total + i
NEXT i
PRINT "somma 1..100 ="; total


NOTEBOOK EXPORT media.bas     ' salva la parte BASIC come programma vero


## Cosa NON fa (per ora)

## Riferimenti

### OutputCapture

*Fonte: `OutputCapture`*


## Panoramica

## Componenti

### `GMBasicSharp/common/CommonLib/Consoles/TeeWriter.cs`

### `Executor.CapturedOutput` (`Executor.cs:210`) — legacy C#

### `EnsureCaptureActive()` (`Repl.Commands.cs:1388`) — legacy C#

### Comando REPL `CAPTURE` (`Repl.Commands.cs:1411`) — legacy C#

## Flusso

<!-- END SURFACE -->
