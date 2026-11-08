# III.15 — Testare i propri programmi

## Cosa ottieni da questo capitolo

Un modo per sapere che il tuo programma fa quello che dici, senza guardare l'output a occhio.
Il progetto usa lo stesso metodo per le sue 1.540 funzioni: un file, un oracolo, un esito
netto. È il capitolo che rende utile tutto il resto.

## 1. L'oracolo nel file

Un test è un programma BASIC con una riga che dichiara cosa deve uscire:

```basic
' EXPECT: barewait-ok|
```

`EXPECT` è un commento: il compilatore lo ignora, il runner lo legge. Se l'output del programma
**contiene** quella stringa, il test passa. Più righe `EXPECT` = più stringhe da trovare, tutte.

La forma `' EXPECT:` (o `REM EXPECT:`) è l'unica riconosciuta. Il testo è confrontato così com'è,
spazi compresi: per questo gli esempi del libro copiano gli output senza abbellirli.

## 2. Dove vivono i test

```
shared/minitests/<dominio>/<nome>.bas
```

Un file per caso, nella cartella del dominio (`concurrency/`, `bits/`, `vectordb/`, `ffi/`…).
Il programma gira con la **cartella dei minitest come directory corrente**: i percorsi relativi
dentro il test si scrivono da lì, non dalla cartella del file.

## 3. Eseguire un solo test, sul backend nativo

```bash
GMB_MINI_ONLY="concurrency/gosub_bare_wait.bas" tools/sh/test_minitests_c99native
```

Output reale:

```
runtime C: compilo (375 sorgenti)...
MiniTest: .../shared/minitests
PASS concurrency/gosub_bare_wait.bas :: barewait-ok|
     C generato: tools/out/c99native/gosub_bare_wait.c
---------------------------------
C99Native MiniTests: PASS=1 FAIL=0 SKIP=0
```

Tre informazioni utili: il verdetto con la stringa cercata, il **C generato** (apribile per
capire cosa ha prodotto il transpiler) e il conteggio finale. Exit code diverso da zero se
`FAIL > 0`.

## 4. Lo stesso test su tutti i backend

```bash
zsh tools/sh/test_minitests_parity.sh "concurrency/gosub_bare_wait.bas"
```

Esegue il file su IL, PCodeVM e VM/C con il runner xUnit e su C99Native con il runner nativo,
confrontando gli esiti. Un backend senza build è `SKIP`, non un errore.

Per un **sample** (non un minitest) il confronto si fa con `parity4`, che usa C99Native come
riferimento:

```bash
zsh tools/sh/parity4 ../shared/samples/Concurrency/ChannelBasics.bas
```

## 5. Test che non devono girare da soli

Un test che apre una finestra, attende un tasto o blocca su un evento non può girare in una
suite. Si marca nel file:

```basic
' TAG: interactive
```

Il runner lo **salta** e lo dice; per eseguirlo serve `GMB_RUN_INTERACTIVE=1`. Il marcatore sta
nel file, non in una lista: chi scrive il test sa se è interattivo, il runner non deve indovinare.

## 6. La trappola: compilare non è funzionare

Il controllo di conformità del progetto compila tutti i sample e produce il bytecode: è un
metro di copertura, non una verifica di comportamento. Un programma che compila e restituisce
`0` dove serve `30` passa comunque. Per questo ogni esempio di questo libro è stato **eseguito**,
non solo compilato — e per questo il capitolo insiste sull'oracolo.

## 7. Regola pratica: un file, un backend

Un test che tocca un file nuovo si esegue su quel file, sul backend che si sta modificando:
C99Native per il transpiler, IL per il runtime gestito. Gli sweep di dominio (tutti i test di
`concurrency/`, tutti i sample) si lanciano quando servono davvero e con il consenso di chi
lavora sullo stesso albero: costano minuti, scaldano la macchina e confondono il segnale con il
rumore delle altre sessioni.

## 8. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| `FAIL` con oracolo giusto | l'output ha spazi o maiuscole diverse | copia la riga dall'output reale |
| `FAIL` senza messaggio | il programma è uscito prima della stampa | controlla `exit` e le righe precedenti |
| il test passa a mano e fallisce nel runner | dipende dalla CWD | usa percorsi relativi a `shared/minitests` |
| il test passa su un backend e non su un altro | divergenza reale: è il motivo per cui esiste il test | apri il caso, non "aggiusta" l'oracolo |
| il runner si blocca | test interattivo non marcato | aggiungi `' TAG: interactive` |

## 9. Dove andare adesso

- **III.14** — leggere un errore e trovare la riga.
- **III.12** — cosa produce il transpiler, cioè quel `C generato`.
- **VI.37** — distribuire il codice che hai testato.



<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### Testing-Guide

*Fonte: `Testing-Guide`*


## ⛔ REGOLA PRIMARIA — classificazione dei sample PRIMA di eseguire test headless

## 🆕 Strumenti attuali (repo unificato)
| Strumento | Cosa fa | Esecuzione |
|---|---|---|
| **Conformance completo** | Compila ricorsivamente i `.bas` sotto `shared/samples/`, escluso `shared/samples/database/`, + codegen P-Code (chunk A1). Metro di copertura (D11) | `dotnet run --project tools/runners/GMBASIC.Conformance -- <filtro>` |
| **vmtest (VM/C bridge)** | Compila **un** file, serializza chunk A1 (GMBC), lo esegue con `gmb_vm_host` (VM/C). Mostra l'output reale | `dotnet run --project tools/runners/GMBASIC.Conformance -- vmtest <file.bas> [<hostPath>]` |
| **Build VM/C** | Compila il binario nativo della VM (in `GMBasic/native/gmb_vm_c`) | da `GMBasic/native/gmb_vm_c` (o `GMBASIC_VM_HOST`) |
| **Gate IL (backend)** | Backend IL AOT (`GMBasicSharp/common/IlBackendRuntime`) — parte della matrice build (Community/Commercial) | via `dotnet build/publish` con `GmbBackends=IL` |
| **Gate C99 (Commercial only)** | Transpiler AST→C — **solo build Commercial** (D4) | vedi [[01-Projects/GMBasicMonoRepo/_archive/C99Native/Guide/C99-Transpiler-Test-Guide]] |

## ⚠️ Strumenti legacy (per-repo, ritirati / deprecati)
| Strumento | Cosa faceva | Stato |
|---|---|---|
| `test_exec` | Compila + esegue un file (AST executor), opzioni di debug | ❌ Legacy/ritirato |
| `test_exec_all` | Compila + esegue tutti i sample, uno alla volta (sequenziale) | ❌ Legacy/ritirato |
| `test_run` | Solo compilazione (lexer+parser+linker) di tutti i sample | ❌ Legacy/ritirato (sostituito dal Conformance) |
| `test_dasm` | Compile + disassemble P-CODE + unit test interni | ❌ Legacy/ritirato (il disassembler è legacy) |
| `test_debug` | Debug interattivo con tracing istruzione-per-istruzione | ❌ Legacy/ritirato |
| `test_exec_dbf` / `test_exec_sql` | Gate runtime su sample DBF/SQL | ❌ Legacy/ritirato |
| `test_island` | Golden test island-based (solo GMBasicAI) | ❌ Legacy/ritirato (golden file vietati, vedi [[Guide/Test-Procedure]]) |
| `test_il` / `IlBackend.SmokeTest` | Test IL backend (solo GMBasicAI) | ❌ Legacy/ritirato come runner; il backend IL è attivo in `GMBasicSharp/common/IlBackendRuntime` |

## Come si legge l'esito del Conformance (per ogni sample)

  PASS [Categoria/File.bas]
  ERROR [Categoria/File.bas] <primo messaggio di errore>


### ⚠️ Blind spot noto (vale anche per il Conformance): "OK ≠ corretto"

dotnet run --project tools/runners/GMBASIC.Conformance -- vmtest shared/samples/<Categoria>/<file>.bas


## `vmtest` — debug di un singolo file via VM/C

dotnet run --project tools/runners/GMBASIC.Conformance -- vmtest <file.bas> [<hostPath>]
# hostPath: GMBasic/native/gmb_vm_c (o GMBASIC_VM_HOST)


  code=N bytes, consts=N, entry=N
  A1 chunk: /tmp/gmb_vmtest_<hex>.chunk (N bytes)
  ---- VM/C OUTPUT ----
  <output reale del programma>
  exit=N


## Marker EXPECT

' EXPECT: Test completato
' EXPECT: 3 OK


## Interpretazione dei risultati legacy (riferimento storico)

## Flusso di lavoro consigliato prima di un commit (unificato)

## Riferimenti

### Test-Index-and-Runners

*Fonte: `Test-Index-and-Runners`*


## 1. La mappa dei test_* (runner)
| Runner | Path | Backend | Scopo | Comando | Note |
|---|---|---|---|---|---|
| `test_ilrun` | `test_ilrun/` (root, g-i) | **IL** (default) · `--engine=pcode` | esegue UN sample | `dotnet run --project test_ilrun -- shared/samples/...` | env `GAI_NO_ILOPT`/`GAI_METHOD` |
| `test_pcode` | `test_pcode/` (root, g-i) | **PCodeVM C#** | esegue UN sample | `dotnet run --project test_pcode -- <sample>` | default `PCodeGpmRwMutex.bas`; plugin-cmd/mkt/test |
| `test_mlx_bench` | `test_mlx_bench/` (root, g-i) | **MLX nativo** (P/Invoke) | **benchmark prefill/decode** dei modelli embedded MLX | `dotnet run --project test_mlx_bench -- <modelDir> prefill <chunk> <N> \| decode <Ngen>` | linkato a GMBasic.sln (file untracked); es. `-- ~/ai/Models/Llama-3.2-1B-Instruct-4bit prefill 2048 8192`; baseline documentata nel Program.cs |
| `test_ilrun-sweep` | `test_ilrun-sweep/` (root, g-i) | **IL** | sweep con filtro | `dotnet run --project test_ilrun-sweep -- <filtro>` | **filtro obbligatorio**; timeout 5s; iniezione input (`.input`/QueueInkey); `SWEEP_SKIP` |
| `test_run_all` | `test_run_all/` (root, g-i) | **IL** | runner per classi + valutazione | `RUN_FILTER=<x> dotnet run --project test_run_all` | **RUN_FILTER obbligatorio**; timeout 8s; classi 1-4; `.input`; server helper |
| `test_graphics_headless` | `test_graphics_headless/` (root, g-i) | **IL/VMC/C99** (`--backend`) | grafica headless (SDL dummy) | `./test_graphics_headless <file> [--backend il\|vmc\|c99] --key <c> --keys <n> --hold <ms> --out <bmp>` | **iniettare `--key` per i WAITKEY** (classe 2/4) |
| `test_graphics_visible` | `test_graphics_visible/` (root, g-i) | **IL/VMC/C99** | grafica con finestra reale | `./test_graphics_visible <file> [--backend ...]` | cocoa Retina 1280×960 |
| `test_editor` | `test_editor/` (root, g-i, collegato a GMBasic.sln) | **editor TUI** (render, nessun backend BASIC) | **render headless del `GWBasicEditor`** su `ScreenBuffer` 80×25 + verifica layout (titolo, menu, righe) | `dotnet run --project test_editor` | adattato da GM-Basic; verifica contenuto renderizzato con exit code (golden test testuale); referenzia REPL (dove vive `GWBasicEditor`) |
| `test_vmrun` | `test_vmrun/` (root, g-i, collegato a GMBasic.sln) | **VM/C** (`gmb_vm_host`) | esegue UN sample via VM/C con **timeout + cleanup** (Chromium CDP 9222, gmb_vm_host, profili /tmp) | `dotnet run --project test_vmrun -- <file.bas> [timeout_secondi]` | wrap del `vmtest`; **per sample che possono appendersi** (browser/WS/FETCH); kill garantito + residui ripuliti |
| `test_minitests_parity` | `tools/` (versionato, spostato da root 2026-09-07) | **IL/PCode/VMC** (xUnit filtrato) | parity MIRATA della suite `shared/minitests/` (~60 file, certificazione builtin cross-backend) | `tools/sh/test_minitests_parity.sh <sottostringa>` \| `tools/sh/test_minitests_parity.sh --all` | **mai suite intera per errore**: il runner xUnit esce senza fare nulla se non è settata `GMB_MINI_ONLY`/`GMB_RUN_ALL=1`; `GMB_MINITEST_BUILD=1` forza la build; dettaglio: [[(solved) - MiniTests-5Backends]] |
| `test_minitests_c99native` | `tools/` (versionato, spostato da root 2026-09-07) | **C99Native** (driver `gmbnative`) | esegue TUTTA `shared/minitests/` col compilatore nativo C99 (lex→parse→emit→cc→run) | `tools/sh/test_minitests_c99native` | build il driver al primo run se assente; **percepito pesante**, non rilanciare senza motivo; complementare alla parity IL/PCode/VMC/C99Native |
| `Conformance vmtest` | `tools/runners/GMBASIC.Conformance` | **VM/C** (`gmb_vm_host`) | esegue UN file via VM/C | `dotnet run --project tools/runners/GMBASIC.Conformance -- vmtest <file>` | richiede `GMBASIC_VM_HOST` (build cmake in `GMBasic/native/gmb_vm_c/build`) |
| `GMBasicSharp/tests/GMBASIC.IlBackend.Tests` | `GMBasicSharp/tests/` (versionato) | **IL** (xUnit) | unit test IL | `dotnet test GMBasicSharp/tests/GMBASIC.IlBackend.Tests` | |
| `GMBasicSharp/tests/AI.Tests` | `GMBasicSharp/tests/` (versionato) | **dominio AI** (xUnit) | unit AI / plugin / MCP / agent | `dotnet test GMBasicSharp/tests/AI.Tests` | fast (~1s) |
| `GMBasicSharp/tests/GMBASIC.BackendSupport.Tests` | `GMBasicSharp/tests/` (versionato) | **trasversale** (xUnit, nessun backend eseguito) | unit test del `BackendImplementationRegistry` condiviso (`GMBasicSharp/common/BackendSupport`) — verifica query per backend, audit NotImplemented, default sempre NotImplemented (mai stub silenziosi) | `dotnet test GMBasicSharp/tests/GMBASIC.BackendSupport.Tests` | annidato nella Solution Folder "tests" in `GMBasicSharp.slnx` dal 2026-09-07 (prima fisicamente in `GMBasicSharp/tests/` ma non raggruppato in Solution Explorer) |
| `test_llm_server` | `test_llm_server/` (root, g-i, collegato a GMBasic.sln) | **MLX nativo** (P/Invoke, stesso motore di `test_mlx_bench`) | server HTTP/SSE OpenAI-compatible ("gemello rapidMLX" slice v1) sopra `libgmb_ai_mlx`+`libLLMToken` | `dotnet run --project test_llm_server` | `GET /v1/models`, `/v1/status`, `POST /v1/completions` (stream/non); `/v1/chat/completions` → 501 (mancano chat-template); un thread solo (MLX vincola lo stream al thread che ha caricato il modello) → 503 se occupato; dettaglio: [[Guide/utente/LLM-Server-OpenAI-Compatibile]] |
| `minitest_batch` | `tools/minitest_batch.sh` (versionato 2026-09-19) | **TUTTE** (IL/PCode/VM-C in-process `--no-build`; C99Native runtime in cache) | harness **BATCH senza ricompilazione**: esegue un filtro su più backend **senza** `dotnet build`/`cmake` (solo `--rebuild` una volta, quando i sorgenti cambiano) | `tools/minitest_batch.sh <filtro> [--legs il,pcode,vmc,c99n] [--rebuild]` | Nasce per evitare gli sweep che ricompilano/rilanciano: usa SOLO artefatti precompilati. **Preferirlo** per verifiche chirurgiche (1 file/1 dominio ristretto). La backend VM/C (`gmb_vm_host`) e il driver C99Native (`gmbnative`) devono essere già buildati; il runtime C99Native è in cache (`librt.a`, invalidata dall'hash sorgenti) |

## Registry backend — generazione (`gmb_audit --registry-*`)

cd <GMBasic>
dotnet build tools/audit/audit.csproj -p:SkipNativeAiLibs=true
dotnet exec tools/audit/bin/Debug/net10.0/gmb_audit.dll --registry-<backend> > GMBasicSharp/common/BackendSupport/Registry/<Backend>.cs
# <backend> = il | pcodevm | vmc | c99 | c99native
# <Backend> = IL | PCodeVM | VMC | C99 | C99Native   (Maiuscole nel nome file/metodo)


## 1bis. Benchmark MLX embedded (`test_mlx_bench`) — a cosa serve, come si usa

# Prefill: N token in chunk di `chunk` token
dotnet run --project test_mlx_bench -- ~/ai/Models/Llama-3.2-1B-Instruct-4bit prefill 2048 8192
#   → PREFILL N=8192 chunk=2048 time=0.78s tok/s=10493.7

# Decode: Ngen token autoregressivi (dopo prefill di 64 token)
dotnet run --project test_mlx_bench -- ~/ai/Models/Llama-3.2-1B-Instruct-4bit decode 64
#   → DECODE gen=64 time=0.52s tok/s=122.5

# Context: degrado a contesto crescente (prefill 1K/2K/4K/8K...)
dotnet run --project test_mlx_bench -- ~/ai/Models/Qwen3.8-27B-8bit context


python3 scripts/bench_service_prefill.py --url http://127.0.0.1:8000/v1 \
  --model <nome> --label <lbl> --lengths 2048 8192 16384 --max-tokens 8 --output <file.json>


## 1bis-bis. Continuous batching — harness parità/throughput (`tools/mlx_batch_test`)

tools/mlx_batch_test.sh parity [n] [gen] [modelDir]   # default n=2, gen=64
tools/mlx_batch_test.sh tput   [n] [gen] [modelDir]   # default n=4, gen=64
# es. tools/mlx_batch_test.sh tput 8 64 ~/ai/Models/Llama-3.2-1B-Instruct-4bit


## 1ter. Harness primitive Qwen4-Exp (`tools/test_qwen4exp_mlx`)

cd tools/test_qwen4exp_mlx
make            # compila tutti i test in ./bin/
make run        # compila e lancia tutti i test in sequenza
make 06_gdn_block.run   # compila e lancia solo un test


## 1quater. ORACOLO = il MiniTest: la riga `' EXPECT:`

' EXPECT: <riga1>|<riga2>|...

| Runner | Comportamento |
|---|---|
| `tools/sh/test_minitests_parity.sh` (IL/PCode/VMC) | `ReadExpect`/`CheckExpect`: PCode, IL e VMC validati **ciascuno** contro EXPECT, oltre che fra loro |
| `tools/sh/test_minitests_c99native` | dopo l'esecuzione confronta l'output con EXPECT → `FAIL … (output != EXPECT)` con atteso/ottenuto stampati |

GMB_MINI_WRITE_EXPECT=1 GMB_RUN_ALL=1 dotnet test GMBasicSharp/tests/GMBASIC.IlBackend.Tests \
    --filter "FullyQualifiedName~MiniTestsParityTests"      # tutti i MiniTest
GMB_MINI_WRITE_EXPECT=1 GMB_MINI_ONLY=io_ dotnet test ...   # solo un dominio


### 1quater-bis. Copertura: "un MiniTest per ogni comando/funzione" (stato 2026-09-11)
| Strumento | Cosa fa |
|---|---|
| `dotnet run --project tools/audit/audit.csproj -- --minisamples` | genera un sample per ogni funzione **PURA** con parametri/ritorno **scalari** (emette `IMPORT <NS>` + `PRINT <NS>.<FN>(valori)`) in `shared/minitests/<ns>/gen_<NS>_<fn>.bas`. `--dry` conta, `--ns X` limita a un namespace |
| `GMB_MINI_WRITE_EXPECT=1 GMB_RUN_ALL=1 dotnet test GMBasicSharp/tests/GMBASIC.IlBackend.Tests --filter "FullyQualifiedName~MiniTestsParityTests"` | **dichiara l'EXPECT** di ogni sample col leg IL **in-process** (un solo processo): `EXPECT: scritti N, gia' corretti M, saltati K` |

### 1quater-ter. Copertura dei COSTRUTTI di linguaggio (non-namespace) — `tools/basic/ast_census.bas`

python3 tools/basic/ast_census.bas                      # report completo (sorgente: all)
python3 tools/basic/ast_census.bas --source minitests   # solo minitests
python3 tools/basic/ast_census.bas --source samples     # solo samples
python3 tools/basic/ast_census.bas --missing            # solo i non coperti
python3 tools/basic/ast_census.bas --covered            # solo i coperti
python3 tools/basic/ast_census.bas --only types         # filtro per categoria (branching/loops/control/declarations/procedures/types/expr/expr-literal/statements/events/concurrency)
python3 tools/basic/ast_census.bas --only class,while    # più categorie o nome costrutto


### ⚠️ Le due trappole del backfill (imparate sul campo)

## 2. Quale runner per quale verifica (flusso)
| Sto modificando… | Verifica | Backend | Dettaglio |
|---|---|---|---|
| Lexer / Parser / Linker / AST | `Conformance -- <filtro>` (compile IL+PCode) | IL+PCode | il compile gate NON esegue: solo compila |
| Dump AST risolto (debug parser/linker) | `test_astdump -- <file>` | IL+PCode | stampa l'AST gerarchico post-link (`DumpASTHierarchical`) |
| Backend IL | `test_ilrun -- <file>` + `RUN_FILTER=<x> test_run_all` + `IlBackend.Tests` | IL | esegue davvero il Main |
| PCodeVM C# | `test_pcode -- <file>` + `Conformance` (chunk) | PCode | compile gate genera il chunk |
| VM/C | `vmtest <file>` | VM/C | prima build `cmake --build GMBasic/native/gmb_vm_c/build` + `GMBASIC_VM_HOST` |
| VM/C browser/WS/network (sample che può appendersi) | `dotnet run --project test_vmrun -- <file> [timeout]` | VM/C | avvia Chromium CDP (9222) + timeout + cleanup; es. `dotnet run --project test_vmrun -- shared/samples/Browser/BrowserC1C4.bas 45` |
| AI / plugin / MCP / agent | `AI.Tests` + `test_ilrun -- <file>` | dominio AI + IL | `AI PLUGINS`/`AI.PLUGINCALL` sono **solo IL/C#** |
| Motore MLX embedded (`llmengine_mlx.c`) | `test_mlx_bench -- <modelDir> prefill\|decode` | MLX nativo | verifica che prefill/decode **non peggiorino** (gate di regressione) |
| Grafica | `test_graphics_headless <file> --key \r --keys 1` | IL/VMC/C99 | `--backend il\|vmc\|c99` |

## 3. Classificazione dei sample BASIC (classe → runner)
| Classe | Definizione | Come si esegue | Runner |
|---|---|---|---|
| **1 — Autonomo** | termina da solo, senza input/rete/schermo | headless con timeout | `test_ilrun-sweep -- <f>` · `RUN_FILTER=<f> test_run_all` · `Conformance` · `C99.Tests` |
| **2 — Input utente** | `WAITKEY`/`INKEY`/`GET`/`INPUT`/TUI/form | **iniettare tasti** | `test_run_all` (`.input`/`@key`/QueueInkey, stdin redirect) · grafica `--key <c> --keys <n>` |
| **3 — Server/Network** | listener (`HTTP SERVER`, `TCP OPEN`+`ACCEPT`, WebSocket) **senza client interno** | **client helper** sulla porta | `test_run_all` (server helper) |
| **4 — Finestra/GPU** | `SCREEN n`/`SPRITE`/`TICK` con finestra | runner grafico | `test_graphics_headless` / `test_graphics_visible` |

### Sample interattivi: NON skipparli, eseguirli con iniezione

### Sample che richiedono SOLO IL (non PCode/VM-C/C99)
| Categoria | Cosa | Dove |
|---|---|---|
| `AI PLUGINS` / `AI.PLUGINCALL` / `AI.PLUGINSLIST` | comandi e funzioni plugin: **solo backend C#/IL** (divergenza sez. A) | `shared/samples/Ai/AiPlugins.bas` |
| `AIAGENT`/`AGENT ASK` | agenti con completer (fixture nei sample) | `shared/samples/Ai/AgentBasic.bas` ecc. |
| `AI.MCP*`, `AI.SKILLS*` | namespace AI | `shared/samples/Ai/` |
| `AI.CACHESTATS` | KV-cache stats del contesto (agente) — **solo IL/C#** (il C99 lo transpila come no-op) | `shared/samples/Ai/KvCacheStats.bas` |

### Conteggio e categorie (2026-08-27)

## 4. Flag e convenzioni comuni

### Test-Run-All-Guide

*Fonte: `Test-Run-All-Guide`*


## Scopo

## Uso

# Filtro per sottostringa del path del sample (OBBLIGATORIO)
RUN_FILTER=Clipper dotnet run --project test_run_all -p:GmbEdition=Commercial
RUN_FILTER=Ai dotnet run --project test_run_all -p:GmbEdition=Commercial

# Edizione Commercial (community + commercial), edizione + build dei progetti
dotnet run --project test_run_all -p:GmbEdition=Commercial

# Solo community
dotnet run --project test_run_all -p:GmbEdition=Community

# Escludere sample che uccidono il processo (come test_ilrun-sweep)
SWEEP_SKIP=TypesGenericType dotnet run --project test_run_all -p:GmbEdition=Commercial

# Timeout per sample (default 8000 ms)
RUN_TIMEOUT_MS=15000 dotnet run --project test_run_all -p:GmbEdition=Commercial



## Strategia per classe
| Classe | Strategia nel runner |
|---|---|
| **1 — Autonomo** | esecuzione headless diretta con timeout (`HeadlessRuntimeHost` + `IlRuntime.SetCurrent`), come `test_ilrun-sweep` |
| **2 — Input utente** | pre-carica `IlRuntime.QueueInkey` (tasti da file `.input` `@key`/`@line`, fallback Enter) + `Console.SetIn` per `INPUT`/`LINE INPUT` |
| **3 — Server** | esegue + **client helper** automatico che si connette alla porta nota e chiude (sblocca `ACCEPT`) |
| **4 — Finestra/GPU** | delega a `test_graphics_headless` (se trovato) con `--key ESC --keys 5 --hold 15000 --out /tmp/gmb_runall.bmp` |

### Classificazione nel codice

### Input iniettato (classe 2)

# commento
@key X        # tasto singolo → IlRuntime.QueueInkey("X")
@key \r       # Enter
@line Mario, 30   # riga intera → Console.SetIn (INPUT/LINE INPUT)


## Output e verdetto

PASS    — termina e il verdetto è OK
FAIL    — termina ma il verdetto è KO (FALLITO / RISULTATO: FALLITO / ERROR / runtime error)
EMPTY   — termina ma output vuoto (possibile bug entry-point)
TIMEOUT — non termina nel tempo dato (classe 2-4 se non gestiti)
COMPILE — non compila (NotSupported / BasicException / gap noti)
SERVER  — classe 3 non gestita da helper (richiede client esterno manuale)
GFX     — classe 4 senza runner grafico disponibile
SKIP    — escluso via SWEEP_SKIP


## Quando eseguirlo

## Differenza con `test_ilrun-sweep`
| | `test_ilrun-sweep` | `test_run_all` |
|---|---|---|
| Classi | tutte headless (TIMEOUT per interattivi) | strategia per classe (2/3/4 gestiti) |
| Iniezione input | ❌ | ✅ `.input` / `@key` / `@line`, fallback Enter |
| Client helper server | ❌ | ✅ (classe 3, porte note) |
| Runner grafico | ❌ | ✅ delega a `test_graphics_*` (classe 4) |
| Verdetto automatico | solo OK/EMPTY/CRASH | + FAIL su marker di errore |

## Riferimenti

<!-- END SURFACE -->
