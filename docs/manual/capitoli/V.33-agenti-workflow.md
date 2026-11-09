# V.33 — Agenti e workflow

## Cosa ottieni da questo capitolo

Un agente è un ciclo: il modello riceve un obiettivo, sceglie uno strumento, guarda il risultato,
decide il passo successivo. Un workflow è la stessa cosa **senza** la scelta: passi fissi, eseguiti
in un ordine che decidi tu. Sapere quale dei due ti serve è metà del lavoro.

## 1. Agente: il ciclo

```
obiettivo → scelta strumento → esecuzione → osservazione → (ripeti) → risposta
```

Gli elementi che rendono un agente utile o pericoloso:

| Elemento | Ruolo | Rischio |
|---|---|---|
| prompt di sistema | definisce ruolo e limiti | troppo vago = comportamenti imprevedibili |
| strumenti | ciò che può fare | troppo potenti = danno reale |
| memoria | contesto fra i passi | crescita dei costi |
| limite di passi | quando fermarsi | assente = ciclo infinito |
| permessi | cosa è consentito | assente = tutto consentito |

La documentazione di riferimento è `Agents-Manual`; i sample `Ai/AgentBasic.bas`,
`AgentMemoryAndPermissions.bas`, `AgentIdentityDemo.bas`, `AgentOperatingCenter.bas` coprono il
ciclo, i permessi e l'identità.

## 2. Workflow: il grafo

Un workflow è un DAG: nodi che fanno un lavoro, archi che dicono cosa viene dopo. Non c'è
creatività, c'è ordine — e per questo è verificabile, ripetibile, testabile.

| Guida | Cosa copre |
|---|---|
| `Workflow-Guida-Utente` | comandi e struttura |
| `WorkflowDag-Guide` | grafo, dipendenze, esecuzione |
| `Workflow-Guide` | pattern d'uso |

Regola: se sai già quali passi servono, **non** usare un agente. Un workflow costa meno, sbaglia
meno e si può testare. L'agente serve quando i passi dipendono da ciò che si trova.

## 3. Multi-agente

Più agenti che collaborano (uno pianifica, uno esegue, uno verifica) sono utili quando i ruoli sono
davvero distinti e c'è un protocollo di scambio. `RecursiveMAS-Manual` descrive la ricorsione fra
agenti, `SelfConsistency-Guida-Utente` la votazione su più risposte.

Il costo è moltiplicativo: N agenti che si parlano sono N² messaggi potenziali. Prima di
aggiungere un agente, chiediti se non basta un passo in più.

## 4. Ingestione e code generation

Due agenti specializzati che vale la pena conoscere:

- **ingestion** (`AgentIngestion-Guide`) — legge documenti, li normalizza, li indicizza: è il
  ponte verso V.34;
- **code generation** (`AI-AGENT-CodeGen-Guida-Utente`, `AgentCodeGen-Guide`) — produce codice e lo
  verifica. Il punto critico è che il codice generato va **eseguito e testato**, non creduto.

## 5. Permessi: la parte che nessuno scrive

Un agente con accesso al filesystem può cancellare file. Un agente con accesso alla rete può
esfiltrare dati. Un agente con accesso a un database può scrivere.

Le regole che funzionano:

1. **lista bianca** degli strumenti, non lista nera;
2. **permesso per operazione distruttiva**, non per agente;
3. **limite di passi e di tempo** sempre;
4. **log di ogni chiamata** allo strumento, con input e output;
5. **nessuna credenziale nel prompt** — l'agente chiama uno strumento, non vede la chiave.

## 6. Osservabilità

Un agente che sbaglia non lo dice: produce una risposta plausibile. Per questo servono tracce:
quanti passi, quali strumenti, quanto è costato, cosa ha visto. `Agents-Dashboard-Guida-Utente` e
`AgentL5-Guide` descrivono il monitoraggio. Senza tracce, un agente è una scatola nera che a volte
ha ragione.

## 7. Perché non c'è un output

Un agente richiede un provider attivo e, nei sample, spesso strumenti esterni (filesystem, rete,
browser). In questo ambiente non ci sono: il codice citato è sintassi documentata, **non eseguita**.
La Parte V dichiara questa lacuna invece di simularla — è la stessa scelta dei capitoli IV.19-IV.21.

## 8. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| l'agente gira all'infinito | nessun limite di passi | imponi un massimo |
| risposte incoerenti fra esecuzioni | temperatura alta, contesto variabile | abbassa la temperatura, fissa il prompt |
| l'agente "dimentica" l'obiettivo | contesto troppo lungo | riassumi o usa RAG |
| danni sul filesystem | strumento troppo potente | lista bianca e conferma |
| costi imprevisti | molti passi con contesto grande | traccia i token per passo |

## 9. Dove andare adesso

- **V.32** — i provider che alimentano gli agenti.
- **V.34** — la conoscenza che gli agenti consultano.
- **V.36** — quando lo strumento è un browser.














<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### Agents-Manual

*Fonte: `Agents-Manual`*


## 1. Riferimento rapido
| NOME | SINTASSI | DESCRIZIONE |
|---|---|---|
| `AI AGENT CREATE` | `AI AGENT CREATE nome` | Crea un agente. |
| `AI AGENT DELETE` | `AI AGENT DELETE nome` | Elimina un agente (e la sua sessione). |
| `AI AGENT SAVE` / `LOAD` | `AI AGENT SAVE nome` | Salva/carica un agente su disco. |
| `AI AGENT SET` | `AI AGENT SET nome, chiave [valore]` | Imposta una proprietà (vedi §3). |
| `AI AGENT REGISTER TOOL` | `AI AGENT REGISTER TOOL nome, tool` | Associa un tool MCP all'agente. |
| `AI AGENT SERVE` / `STOP SERVER` | `AI AGENT SERVE 8765` | Avvia/ferma il server HTTP dell'agente (vedi §7). |
| `AI AGENT TRACE` | `AI AGENT TRACE ON\|OFF\|VERBOSE` | Log dell'agente. |
| `AI AGENT ASK STREAM` | `AI AGENT ASK STREAM nome, prompt INTO Sub` | Streaming per token. |
| `AI.AGENTASKSYNC(name, prompt)` | funzione | Chiede in modo sincrono. |
| `AI.AGENTGET(name, key)` | funzione | Legge una proprietà. |
| `AI.AGENTLIST()` | funzione | Elenco agenti. |
| `AI.AGENTSTATUS(name)` | funzione | Stato dell'agente. |
| `AI.AGENTEVENTS()` | funzione | Eventi degli agenti. |

## 2. Ciclo di vita

IMPORT AI WITH ALIAS

AI AGENT CREATE Analista
AI AGENT CREATE Revisore
AI AGENT DELETE Analista          ' rimuove anche la sessione nativa

AI AGENT SAVE Revisore            ' persiste su disco (JSON: Revisore.agent.json)
AI AGENT LOAD Revisore            ' ripristina (JSON)

AI AGENT SERVE Revisore           ' avvia il server (accetta richieste)
AI AGENT STOP SERVER              ' ferma tutti i server
AI AGENT TRACE VERBOSE            ' log dettagliato delle interazioni


### 2ter. Permessi (Sandbox / Supervised / YOLO) ed eventi

### 2bis. Persistenza — JSON (Community) e ISAM (Commercial)
| Store | File | Comando BASIC | API C# | Edizione | Note |
|---|---|---|---|---|---|
| **JSON** | `nome.agent.json` (es. `Revisore.agent.json`) | `AI AGENT SAVE nome` / `AI AGENT LOAD nome` | `AgentServer.Save/Load` → `AgentPersistence.SaveToFile` | Community + Commercial | snapshot versionato `CurrentVersion=1`, `System.Text.Json` |
| **ISAM** | `agents.isdb` (dictionary `name → snapshot`, atomic `tmp+move`) | via `AgentServer.SaveToIsam(name, "agents.isdb")` / `LoadFromIsam` | `AgentServer.SaveToIsam/LoadFromIsam` (`AgentPersistence.Capture` + `agents.isdb`) | **Commercial** (verificato `GmbEdition=Commercial`, `shared/samples/Ai/AgentIsamPersist.bas`) | In Community `SaveToIsam` resta placeholder D11 (file non creato da BASIC); il gate Commercial verifica round-trip `CREATE → SET → SAVE ISAM → DELETE → LOAD ISAM → ASK` con history preservata |

' JSON (sempre)
AI AGENT SAVE MyAgent
AI AGENT LOAD MyAgent

' ISAM (Commercial, C# — BASIC sample usa SAVE/LOAD JSON ma il test C# verifica ISAM)
' C#: AgentServer.Instance.SaveToIsam("BookingAgent", "agents.isdb")
' C#: AgentServer.Instance.LoadFromIsam("BookingAgent", "agents.isdb")


## 3. Proprietà — `AI AGENT SET nome, chiave valore`

AI AGENT SET "Analista", provider valore        ' virgola dopo il nome
AI AGENT SET "Analista", provider, valore        ' virgola anche dopo la chiave
AI AGENT SET "Analista" KEY provider VALUE valore ' clausole esplicite KEY/VALUE

| chiave | valori | default | descrizione |
|---|---|---|---|
| `model` | stringa | `""` | Modello usato (aggiornato automaticamente alla prima ASK). |
| `temperature` | double 0..2 | `0.7` | Temperatura di campionamento. |
| `maxtokens` | int | `2048` | Token massimi per risposta. |
| `systemprompt` | stringa | config AI | Prompt di sistema dell'agente. |
| `provider` | nome provider | — | Provider specifico (altrimenti l'attivo per `%CHAT`). |
| `skills` / `skillsenabled` | ON/OFF | OFF | Inietta l'indice skill nel prompt. |
| `plugins` / `pluginsenabled` | ON/OFF | OFF | Inietta i plugin operativi nel prompt. |
| `memory` / `memoryenabled` | ON/OFF | OFF | Abilita la reflection nella memoria (`AgentReflection`). |
| `sessionstreaming` / `...enabled` | ON/OFF | OFF | Streaming per-token nelle sessioni native. |
| `clienttools` / `...enabled` | ON/OFF | OFF | Tool dei server MCP visibili all'agente. |
| `thinking` / `thinkingenabled` | ON/OFF | OFF | **Quiet-STaR**: il prompt è wrappato con `<|start_of_thought|>...` (l'agente ragiona prima di rispondere). |
| `critic` | stringa | — | Persona critica dell'agente (es. `spec\|edge\|logic`). |

AI AGENT SET Analista, provider, "mlx-coder"
AI AGENT SET Analista, temperature, 0.3
AI AGENT SET Analista, maxtokens, 512
AI AGENT SET Analista, systemprompt, "Sei un analista finanziario conciso."
AI AGENT SET Analista, skills, ON
AI AGENT SET Analista, memory, ON
AI AGENT SET Analista, thinking, ON
AI AGENT SET Analista, thoughttokens, 12
AI AGENT SET Analista, reflection, both
AI AGENT SET Analista, critic, "edge"

' Letture
PRINT AI.AGENTGET("Analista", "model")
PRINT AI.AGENTGET("Analista", "temperature")
PRINT AI.AGENTGET("Analista", "thinking")        ' ON
PRINT AI.AGENTGET("Analista", "thoughttokens")   ' 12


### 3bis. Scelta del modello — esterno (HTTP) vs embedded (in-process)
| `source` di `AI REGISTER` | kind | Mondo |
|---|---|---|
| `"http://localhost:11434"` (+ `param` = modello) | `ollama` | esterno |
| `"https://api.openai.com/v1"` (+ `param` = modello) | `openai` | esterno |
| `"https://api.anthropic.com/v1"` (+ `param`) | `anthropic` | esterno |
| `"ollama:llama3.1"` / `"openai:gpt-4o"` (scheme registry) | dal registry | esterno |
| path locale `*.gguf` | `llamacpp` (o `mlx` con `{"engine":"mlx"}`) | **embedded** |
| cartella MLX (`config.json` + `*.safetensors`) | `mlx` | **embedded** |
| `"hf:<repo>"` (scarica) | embedded | **embedded** |

' --- Registrazione: esterno e embedded, entrambi disponibili ---
AI REGISTER "ext", %CHAT, "http://localhost:11434", "llama3.1"          ' esterno (Ollama HTTP)
AI REGISTER "emb", %CHAT, "/Users/gaetano/ai/Models/Qwen3-4B-4bit"      ' embedded (motore MLX)

' --- Selezione GLOBALE (default per gli agenti senza pin) ---
AI SET ACTIVE %CHAT, "ext"
PRINT "attivo globale: "; AI.ACTIVE(%CHAT)                               ' -> ext

' --- Selezione PER-AGENTE (coesistono) ---
AI AGENT CREATE "A"
AI AGENT CREATE "B"
AI AGENT SET "A", provider, "ext"     ' A → modello esterno via HTTP
AI AGENT SET "B", provider, "emb"     ' B → modello embedded in-process

' La chiamata è IDENTICA: cambia solo il provider risolto.
rA$ = AI.AGENTASKSYNC("A", "Domanda al modello esterno")


## 4. Chiedere all'agente

' Sincrono
a$ = AI.AGENTASKSYNC("Analista", "Qual è il PIL del 2025?")

' Streaming per token (callback INTO Sub)
AI AGENT ASK STREAM Analista, "Racconta una storia breve." INTO OnToken
SUB OnToken(tok$)
    PRINT tok$;             ' stampa i token man mano
END SUB

' Stato / elenco / eventi
PRINT AI.AGENTSTATUS("Analista")
DIM tutti AS ARRAY OF STRING = AI.AGENTLIST()
DIM evt AS ARRAY OF STRING = AI.AGENTEVENTS()


### 4bis. Input multimodale — `AI.AGENTASKFILE`

' Immagine → vision; audio → STT; pdf/testo → estrazione
a$ = AI.AGENTASKFILE("Analista", "foto.jpg", "Descrivi gli oggetti")
a$ = AI.AGENTASKFILE("Analista", "call.wav", "Riassumi la telefonata")
a$ = AI.AGENTASKFILE("Analista", "fattura.pdf", "Estrai totale e IVA")


## 5. Tool (tool-calling loop) — strutturato vs prompt
| Provider | Tipo tool-calling | Come funziona | File |
|---|---|---|---|
| **OpenAI / Anthropic** (remoto) | **strutturato** `IToolCallingCompleter` | `POST /chat/completions` con `tools[]` → `tool_calls` JSON nativi (nessun parsing) | `OpenAiProviders.cs:132`, `AnthropicProviders.cs:110` |
| **Ollama** (locale/remoto, 2026-09-07) | **strutturato** `IToolCallingCompleter` | `POST /api/chat` con `tools[]` → `message.tool_calls` (parsing `arguments` object/string) | `OllamaProviders.cs:44` |
| **LlamaCpp** (locale `llamacpp`, 2026-09-07) | **strutturato** `IToolCallingCompleter` | prompt `## Available tools …` + `ToolCallParser.TryParse` sul testo (stesso formato `{"tool_calls":[...]}`) | `LlamaCppProviders.cs:116` |
| **MLX** (locale `mlx`) | **constrained** `INativeToolDispatchCompleter` | constrained decoding (scelta tool a token, zero JSON) | `MlxProviders.cs` FASE 3 |
| **Fallback** (qualsiasi altro) | prompt + `ToolCallParser` | `## Available tools …` nel system prompt → parsing `tool_calls` nel testo | `AgentToolLoop.cs:88` |

' MCP tool + agent
AI MCP REGISTER TOOL "calc_sum", "CalcSum", "Sum of two integers a and b"
AI AGENT CREATE Assistente
AI AGENT REGISTER TOOL Assistente, "calc_sum"
AI AGENT REGISTER TOOL Assistente, "filesystem.read_file"

' Ollama locale (ora strutturato)
AI REGISTER "ollama-local", %CHAT, "ollama", "{""model"":""llama3.1""}"
AI SET ACTIVE %CHAT, "ollama-local"

' LlamaCpp locale (ora strutturato via prompt)
' AI REGISTER "llama-local", %CHAT, "llamacpp", "{""modelPath"":""/path/to/model.gguf""}"
' AI SET ACTIVE %CHAT, "llama-local"

' alla ASK, se l'agente richiede il tool, il runtime lo esegue e continua il loop
' con tool_calls strutturati (nessun JSON inventato)


### 5bis. Connettori (52) — discovery automatica, senza `AI AGENT REGISTER TOOL`
| Bisogno | Senza connettori (vecchio) | Con connettori (F1) |
|---|---|---|
| Agent vede tool | `AI AGENT REGISTER TOOL Assistente, "calc_sum"` esplicito per ogni agente | `AI CONNECTOR LOAD "gmail"` **una volta** (globale) → tutti gli agenti lo vedono |
| Discovery 300 tool | tutti nel prompt → hallucination | `AI.CONNECTORSEARCH("mail")` → solo `gmail`/`microsoft-365`/`sendgrid` |

IMPORT AI

' Una volta, globale (persistito in config/connectors.json)
AI CONNECTOR LOAD "gmail"
AI CONNECTOR LOAD "filesystem"
AI CONNECTOR LOAD "github"
AI CONNECTOR SECRET SET "gmail", "api_key", "ya29.demo"  ' o AUTH OAuth PKCE

' Ogni nuovo agente li vede automaticamente — nessun REGISTER
AI AGENT CREATE "AutoAgent"
' Con LLM: AI.AGENTASKSYNC("AutoAgent","cerca fatture ACME e lista work/")
' → tool_calls: gmail.search {"query":"from:ACME"} → filesystem.list {"path":"work"}
' Equivalente imperativo (ciò che farebbe l'agente):
DIM r1$ AS STRING = AI.CONNECTORCALL("gmail", "search", "{""query"":""from:ACME""}")
DIM r2$ AS STRING = AI.CONNECTORCALL("filesystem", "list", "{""path"":""work""}")
PRINT r1$ : PRINT r2$


## 6. Esempio complesso — agente con memoria, thinking e reflection

IMPORT AI WITH ALIAS
IMPORT PROVIDER WITH ALIAS

SUB Main()
  ' provider locale reale (MoE 3B attivi)
  AI REGISTER "coder", %CHAT, "mlx", "{""modelDir"":""/Users/gaetano/ai/Models/Qwen3-Coder-30B-A3B-Instruct-8bit""}"
  AI SET ACTIVE %CHAT, "coder"

  AI AGENT CREATE Progettista
  AI AGENT SET Progettista, provider, "coder"
  AI AGENT SET Progettista, systemprompt, "Sei un architetto software senior. Pensa prima di rispondere."
  AI AGENT SET Progettista, thinking, ON
  AI AGENT SET Progettista, thoughttokens, 16
  AI AGENT SET Progettista, memory, ON
  AI AGENT SET Progettista, reflection, both



## 7. HTTP API — Agent Server (`AI AGENT SERVE`)
| Route | Metodo | Body / Query | Risposta | File |
|---|---|---|---|---|
| `POST /chat` | POST | `{"agent":"name","prompt":"..."}` | `{"answer":"..."}` | `IlRuntime.cs:7528` |
| `POST /execute` | POST | `{"agent":"name","prompt":"..."}` o `{"agent":"name","tool":"calc_sum","args":{}}` | `{"answer":"..."}` o `{"tool":"...","result":"...","isError":false}` | `IlRuntime.cs:7544` |
| `POST /workflow` | POST | `{"workflow":"name"}` (o `{"name":"..."}`) | `{"workflow":"...","status":"completed","steps":"s1,s2","log":[...],"executed":true}` | `IlRuntime.cs:7590` |
| `POST /memory` | POST | `{"agent":"name"}` o `{"agent":"name","query":"..."}` | `{"agent":"...","status":"...","history":[...],"memory":[...]}` | `IlRuntime.cs:7620` |
| `GET /memory` | GET | `?agent=name` (o `?agent=name&query=...`) | stesso di POST | `IlRuntime.cs:7620` |
| `GET /api/agents` | GET | — | `[{name,status,permission}]` (JSON array, `AgentStatusManager`) | `IlRuntime.cs` (C1, 2026-09-07) |
| `GET /api/teams` | GET | — | `[{name,strategy,memberCount}]` | `IlRuntime.cs` (C1, 2026-09-07) |
| `POST /api/agent/{name}/prompt` | POST | `{"prompt":"..."}` | `{"agent":"...","answer":"..."}` | `IlRuntime.cs` (C1, 2026-09-07) |

AI AGENT CREATE Demo
AI WORKFLOW DemoWF
AI WORKFLOW STEP s1, "step one"
END WORKFLOW
AI AGENT SERVE 8765
' in un altro terminale:
' curl -X POST http://127.0.0.1:8765/chat -H "Content-Type: application/json" -d '{"agent":"Demo","prompt":"Hello"}'
' curl -X POST http://127.0.0.1:8765/execute -d '{"agent":"Demo","prompt":"Hello"}'
' curl -X POST http://127.0.0.1:8765/workflow -d '{"workflow":"DemoWF"}'
' curl "http://127.0.0.1:8765/memory?agent=Demo"
' curl http://127.0.0.1:8765/api/agents
' curl http://127.0.0.1:8765/api/teams
' curl -X POST http://127.0.0.1:8765/api/agent/Demo/prompt -d '{"prompt":"Hello"}'
SLEEP 5
AI AGENT STOP SERVER


## 8. Coda e Canali — Queue/ACF (L1, 2026-09-07)
| Comando | Sintassi | Descrizione |
|---|---|---|
| `AI AGENT SEND` | `AI AGENT SEND To, Task, Input [, Payment]` | Enqueue (From=`"user"`). Ritorna `-1` se ok, `0` se coda piena/stoppata |
| `AI AGENT RECEIVE` | `AI AGENT RECEIVE Agent INTO Var$` | Dequeue → JSON `AgentMessage` o `""` se vuota/pausata |
| `AI AGENT PAUSE` | `AI AGENT PAUSE Agent` | Pausa la coda (RECEIVE bloccato, SEND ancora in coda) |
| `AI AGENT RESUME` | `AI AGENT RESUME Agent` | Riprende |
| `AI AGENT STOP` | `AI AGENT STOP Agent` | Ferma + svuota, nuovi SEND rifiutati |
| `AI CHANNEL CREATE` | `AI CHANNEL CREATE Chan` | Crea canale (no-op, auto-creato su PUBLISH) |
| `AI CHANNEL PUBLISH` | `AI CHANNEL PUBLISH Chan, Message` | Pubblica su tutti i subscriber |
| `AI CHANNEL SUBSCRIBE` | `AI CHANNEL SUBSCRIBE Chan, Handler` | **No-op di parità**: registra nulla, ritorna 1 |
| `AI CHANNEL UNSUBSCRIBE` | `AI CHANNEL UNSUBSCRIBE Chan, Handler` | **No-op di parità**: ritorna 1 |

IMPORT AI WITH ALIAS
AI AGENT CREATE Worker
AI AGENT SEND Worker, "Task1", "Analizza doc.pdf"
AI AGENT SEND Worker, "Task2", "Genera report", 5
DIM msg$ AS STRING
AI AGENT RECEIVE Worker INTO msg$   ' {"Id":"...","Task":"Task1",...}
AI AGENT PAUSE Worker
AI AGENT SEND Worker, "Task3", "In pausa" ' in coda ma non consumabile
AI AGENT RECEIVE Worker INTO msg$   ' "" (pausata)
AI AGENT RESUME Worker
AI AGENT RECEIVE Worker INTO msg$   ' Task2
AI AGENT STOP Worker                ' svuota
AI CHANNEL CREATE MyChan
AI CHANNEL PUBLISH MyChan, "Hello"
AI CHANNEL SUBSCRIBE MyChan, "Handler"
AI CHANNEL UNSUBSCRIBE MyChan, "Handler"


## 9. Riferimenti

### AI-AGENT-CodeGen-Guida-Utente

*Fonte: `AI-AGENT-CodeGen-Guida-Utente`*


## Comandi

AI AGENT CODE "Scrivi una FUNCTION Somma(a AS INTEGER, b AS INTEGER) AS INTEGER che ritorna a+b", "work/somma.bas"
AI AGENT FIX "Errore di compilazione: ...", "work/somma.bas"


## Comportamento

## Uso con gestione errori

ON ERROR RESUME NEXT
AI AGENT CODE "…", "work/gen.bas"
IF ERR = 0 THEN PRINT "generato" ELSE PRINT "non disponibile (nessun LLM)"
ON ERROR GOTO 0


### AgentCodeGen-Guide

*Fonte: `AgentCodeGen-Guide`*


## 1. BrainVaultBuilder (B2.1)

' C# API (usata da IlRuntime.BrainVaultBuild)
' WORK è la cartella di output
built = AI.BRAINVAULTBUILD("work")        ' → work/brain/AI.md, work/brain/TORCH.md, ... + brainvault.db
q$ = AI.BRAINVAULTQUERY("calcola VaR", 3)  ' → JSON hits (TRADING.METRICS.VALUEATRISK)
code$ = AI.BRAINVAULTWRITE("Scrivi una FUNCTION Somma", 3) ' → BASIC generato


## 2. CodeGenTools (B2.2)
| Metodo | Firma | Sandbox |
|---|---|---|
| `ToolWrite` | `ToolWrite(path$, content$, sandbox)` | `Ask`→log, `Auto`/`Yolo`→scrive |
| `ToolCompile` | `ToolCompile(path$)` → `(ok, diagnostics$)` | compila via `Orchestrator.Compile` (reflection, no ciclo) |
| `ToolRun` | `ToolRun(path$)` → `(ok, output$, diagnostics$)` | compila + `IlProgramCompiler` → `GeneratedProgram.Main` |
| `ToolExecute` | `ToolExecute(path$, content$, sandbox)` | Write→Compile→Run in uno |

' BASIC non ha ToolWrite diretto — è per CodeGenLoop/C#; in BASIC si usa AI AGENT CODE


## 3. CodeGenLoop (B2.3)

var loop = new CodeGenLoop(LlmCompleterResolver.Default, maxFixAttempts: 3);
var (ok, path, output, diag) = loop.Run("Scrivi una FUNCTION Somma(a AS INTEGER, b AS INTEGER) AS INTEGER", "work/gen.bas");


## 4. Parser AI AGENT CODE/FIX (B2.4)

' Legacy a spazio, senza virgole per la chiave, ma con virgola per i parametri CODE/FIX
AI AGENT CODE "Scrivi una FUNCTION Somma(a AS INTEGER, b AS INTEGER) AS INTEGER", "work/gen.bas"
AI AGENT FIX "Token '[' inatteso", "work/gen.bas"


## 5. Verifica

# BrainVault
dotnet test --filter BrainVaultTests --verbosity minimal  # 5/5

# CodeGenLoop con mini-LLM (fixture)
# (harness `CheckCodeGen` rimosso: oggi BrainVault/CodeGen si verificano con i MiniTest `ai/*` e
#  con `tools/runners/AI.Eval`)

# Sample L2
GMB_MINI_ONLY="ai/" tools/sh/test_minitests_parity.sh   # COMPILE OK


## 6. Riferimenti

### AgentIngestion-Guide

*Fonte: `AgentIngestion-Guide`*


## 1. IngestionBridge (B3.1)

var (chunks, indexed, sourceId) = IngestionBridge.Import("/tmp/doc.pdf", "note", "kb", "docs");
var hits = AiVectorRuntime.SearchJson("kb", "docs", "query", 3, false, null);


## 2. SkillRegistry (B3.2)

SkillRegistry.RequireSkill("github:user/repo");  // o "user/repo"
SkillRegistry.Publish("quant", "indicatori quantitativi", "github:user/quant");
var hits = SkillRegistry.Search("quant", 5);      // List<SkillRecord>
var json = SkillRegistry.SearchJson("quant", 5);  // {query, count, hits}


## 3. StructuredRunner (B3.3)

var schema = "{\"type\":\"object\",\"properties\":{\"field\":{\"type\":\"string\"}},\"required\":[\"field\"]}";
var (ok, json, err) = StructuredRunner.RunStructured("prompt", schema, completer);
// ok=true → json valido; ok=false → json=testo grezzo, err=errore esplicito


## 4. Copertura pre-esistente

## 5. Verifica

dotnet test --filter IngestionTests --verbosity minimal  # 4/4
dotnet test --filter "Category=Fast" --verbosity minimal  # 435/435


### AgentL5-Guide

*Fonte: `AgentL5-Guide`*


## 1. AgentIdentity (B5.1) — KEYPAIR secp256k1

var pub = AgentIdentity.AttachKeypair(agent);        // → PUBLIC KEY PEM
var sig = AgentIdentity.Sign(agent, "msg");          // → base64 SHA256withECDSA
var ok  = AgentIdentity.Verify(agent, "msg", sig);   // → true/false
var (priv, pub) = AgentIdentity.CreateKeypair();     // standalone


## 2. AgentStatusManager (B5.3) — JSON

AgentStatusManager.GetAgentStatusJson("analista")   // {name,id,model,status,permission,toolCount,historyCount,hasKeypair,teams,updatedAt}
AgentStatusManager.GetTeamStatusJson("Squadra")     // {name,id,status,strategy,coordinator,memberCount,members,updatedAt}
AgentStatusManager.GetAgentsJson() / GetTeamsJson() // array


## 3. LanguageManifest (B5.5) — GMBASIC_LANGUAGE.json + STRICT

LanguageManifest.WriteTo("work/gmbasic_language.json");  // 63 ns, ~1300 funzioni StdLib
LanguageManifest.StrictMode = true;                       // AI STRICT ON (limita tool ai namespace dichiarati)


## 4. Copertura pre-esistente / parziale

## 5. Verifica

dotnet test --filter "AgentIdentityTests|AgentStatusManagerTests" --verbosity minimal  # 7/7
dotnet test --filter "Category=Fast" --verbosity minimal                              # 446/446


## 6. Riferimenti

### Agents-Dashboard-Guida-Utente

*Fonte: `Agents-Dashboard-Guida-Utente`*


## Concetto

## API principali
| Endpoint | Metodo | Cosa restituisce |
|---|---|---|
| `GET /api/agents` | GET | lista agenti `[{name,status,permission}]` |
| `GET /api/teams` | GET | lista team `[{name,strategy,memberCount}]` |
| `POST /api/agent/{nome}/prompt` | POST | invia un prompt all'agente, risposta `{"answer":…}` |
| `GET /memory?agent=<nome>` | GET | memoria/stato agente |
| `POST /workflow` | POST | esegue un workflow `{"workflow":nome}` |

## Uso

curl http://127.0.0.1:8765/api/agents
curl -X POST http://127.0.0.1:8765/api/agent/Demo/prompt -d '{"prompt":"Ciao"}'


### RecursiveMAS-Manual

*Fonte: `RecursiveMAS-Manual`*


## 1. Comandi — `AI TEAM`
| Comando | Effetto |
|---|---|
| `AI TEAM CREATE nome` | crea un team |
| `AI TEAM nome ADD AGENT agente [AS ruolo]` | aggiunge un membro |
| `AI TEAM nome REMOVE AGENT agente` | rimuove un membro |
| `AI TEAM nome SET ROLE agente ruolo` | Coordinator/Worker/Critic/Observer |
| `AI TEAM nome SET STRATEGY s` | `sequential\|roundrobin\|coordinator\|mixture\|distillation\|deliberation` |
| `AI TEAM nome SET RECURSION n` | round ricorsivi `r=1..8` (R1) |
| `AI TEAM nome SET LATENT mode` | `text\|embed\|native` (R1/R2/R5) |
| `AI TEAM nome SET LINK inner, "{\"hidden\":64}"` | link interno (R_in residual, A.9) |
| `AI TEAM nome SET LINK outer, "{\"hidden\":64}"` | link esterno (R_out) |
| `AI TEAM nome SET THOUGHTS m` | latent thoughts per agente `1..32` |
| `AI TEAM TRAIN INNER team, agent, dataset` | training inner (coseno) — R4 |
| `AI TEAM TRAIN OUTER team, dataset` | training outer (joint) — R4 |
| `AI TEAM SAVE nome` / `AI TEAM LOAD nome` | checkpoint team **+ pesi R_in/R_out** (A.4) |
| `AI TEAM MESSAGE nome, testo` | messaggio nel team |

## 2. Funzioni — `AI.TEAM*`
| Funzione | Tipo | Significato |
|---|---|---|
| `AI.TEAMLIST()` | STRING | elenco team (righe `\n`) |
| `AI.TEAMAGENTS(name)` | STRING | agenti del team |
| `AI.TEAMTRAINSTATUS(name)` | STRING (JSON) | `{team, inner:{agent:{steps,loss}}, outer:{steps,loss}}` |
| `AI.TEAMLINKINFO(name)` | STRING (JSON) | `{team, inner:{hidden}, outer:{hidden}}` |
| `AI.TEAMRECURSION(name)` | LONG | `r` configurato |
| `AI.TEAMLATENT(name)` | STRING | modalità latente |
| `AI.TEAMRUNTIMESTATS(name)` | STRING (JSON) | `{team, runs:[{round,timeMs,tokens,asks}], lastRunMs, lastRunRounds}` |

## 3. Modalità latente (R2/R5)

IMPORT AI WITH ALIAS
SUB Main()
  AI AGENT CREATE Planner
  AI AGENT CREATE Solver
  AI TEAM CREATE Squadra
  AI TEAM Squadra ADD AGENT Planner
  AI TEAM Squadra ADD AGENT Solver
  AI TEAM Squadra SET STRATEGY sequential
  AI TEAM Squadra SET RECURSION 2
  AI TEAM Squadra SET LATENT native
  AI TEAM Squadra SET LINK inner, "{""hidden"":64}"
  AI TEAM Squadra SET LINK outer, "{""hidden"":64}"
  AI TEAM Squadra RUN "Qual è la capitale dell'Italia?"
  PRINT AI.TEAMRUNTIMESTATS("Squadra")
END SUB
Main()


## 4. Training (R4)

## 5. Persistenza (A.4/A.16)

## 6. Sample

## 7. Test

# suite veloce del dominio AI — NIENTE modelli reali (i test real-model skippano):
dotnet test GMBasicSharp/tests/AI.Tests --filter "Category=Fast" -p:RunAnalyzers=false


## 8. Stato e residui

### SelfConsistency-Guida-Utente

*Fonte: `SelfConsistency-Guida-Utente`*


## Panoramica

## Le 3 funzioni

IMPORT AI

' (a) Metrica PURA su risposte GIÀ ottenute — deterministica, senza provider
e = AI.LEXICALENTROPY("42;42;42;42;42")   ' 0      (unanime)
e = AI.LEXICALENTROPY("42;24;13;7;99")    ' 2.3219 (tutte diverse = log2(5))
e = AI.LEXICALENTROPY("The answer is 42;42;42.0;seven;7")  ' 0.971 (split 3-2)

' (b) Campionamento K× da provider LLM + risk 0..1 (richiede %CHAT REALE attivo)
risk = AI.CONSISTENCY("Qual è la capitale dell'Australia?", {"k": 5, "temp": 0.7})
'      senza provider REALE restituisce NaN (il provider "fixture" di bootstrap NON conta)

' (c) Report strutturato dell'ultima misurazione (JSON: k, cluster, entropia, verdict)
d$ = AI.CONSISTENCYDETAIL$()
'      {"k":3,"n_buckets":2,"entropy":0.918296,"risk":0.579380,"collapse_warning":false,...}


## Funzioni

### `AI.LEXICALENTROPY(responses$)` → `double`

' responses$ = stringhe separate da ';' o newline
e = AI.LEXICALENTROPY("42;42;42;42;42")       ' → 0.0 (consenso)
e = AI.LEXICALENTROPY("42;24;13;7;99")        ' → log2(5) ≈ 2.3219 (massima divergenza)
e = AI.LEXICALENTROPY("The answer is 42;42;42.0;seven;7")
                                               ' → ~0.971 (split 3-2 dopo normalizzazione)


### `AI.CONSISTENCY(prompt$, options?)` → `double`

' Opzioni (tutte opzionali):
'   k         = numero risposte (default 5, max 32)
'   temp      = temperatura sampling (default 0.7)
'   maxTokens = token max per risposta (default 1024)
'   model     = nome provider specifico (default: active %CHAT)

risk = AI.CONSISTENCY("Qual è la capitale dell'Australia?", {"k": 5, "temp": 0.7})
' → risk ∈ [0,1]  (NaN se nessun provider attivo)


### `AI.CONSISTENCYDETAIL$()` → `string`

d$ = AI.CONSISTENCYDETAIL$()
' {
'   "k": 5,
'   "n_buckets": 2,
'   "entropy": 0.970951,
'   "risk": 0.418166,
'   "collapse_warning": false,
'   "buckets": [
'     {"size": 3, "repr": "42"},
'     {"size": 2, "repr": "7"}
'   ]
' }


## Lettura del risk

## Confident Mode Collapse (trappola >100B)

## Envelope d'uso (dal paper di riferimento)

## Esempio Completo

' 1. Verifica consenso su risposte già ottenute (deterministico)
resp$ = "42;The answer is 42;42.0;Answer: 42;forty two"
e = AI.LEXICALENTROPY(resp$)
print "Entropia: "; e

d$ = AI.CONSISTENCYDETAIL$()
print "Detail: "; d$

' 2. Se hai provider LLM: campiona e valuta
AI SET ACTIVE %CHAT, "mio-llama3"
risk = AI.CONSISTENCY("2+2=?", {"k": 5, "temp": 0.7})
if risk = risk then  ' non NaN
    if risk < 0.3 then
        print "Consenso forte → affidabile"
    elseif risk < 0.7 then
        print "Divergenza moderata → verifica"


## Evidenza locale — pilota AUROC GSM8K (2026-09-15)

## Triage automatico in agente (P3)

## Parità 4 Backend
| Backend | Implementazione | Note |
|---------|----------------|------|
| **IL** | C# puro (`SemanticEntropy.cs`) | `AskMulti` su `ILlmCompleter` |
| **PCodeVM** | C# puro (`SemanticEntropy.cs`) | Stesso di IL |
| **VM/C** | Runtime C `gmb_uncertainty.c` | Builtin 3538-3540 |
| **C99Native** | Runtime C `gmb_uncertainty.c` | Emitter RE_TBL → `gmb_consistency()` |

## MiniTest

# Esegui su singolo backend
dotnet run --project GMBasicSharp/tests/GMBASIC.IlBackend.Tests -- --filter "MiniTest.AiConsistency"

# Parità 4 backend
tools/sh/parity4 shared/minitests/ai/consistency.bas


## Verifiche

## Riferimenti

## Note Implementative

### Workflow-Guida-Utente

*Fonte: `Workflow-Guida-Utente`*


## Definire un workflow

AI WORKFLOW <nome>          ' inizia la definizione
AI WORKFLOW STEP <etichetta>, "<azione>"
AI WORKFLOW STEP <etichetta2>, "<azione2>"
END WORKFLOW                ' chiude la definizione


## Eseguire e ispezionare

AI WORKFLOW EXECUTE <nome>              ' esegue i passi in sequenza (stato 'completed')
DIM s AS STRING
s = AI.WorkflowSteps("<nome>")          ' passi definiti (JSON/lista)
s = AI.WorkflowList()                   ' workflow registrati
s = AI.WorkflowStatus("<nome>")         ' stato (ready/completed/…)


## Note

### Workflow-Guide

*Fonte: `Workflow-Guide`*


## 1. Riferimento rapido
| Nome | Sintassi | Descrizione |
|---|---|---|
| `AI WORKFLOW` | `AI WORKFLOW nome` | Inizia definizione workflow `nome` |
| `AI WORKFLOW STEP` | `AI WORKFLOW STEP s, azione [, retry [, timeoutMs]]` | Aggiunge passo `s` con azione `azione` + retry/timeout opzionali |
| `END WORKFLOW` | `END WORKFLOW` | Chiude definizione (`ready` o `empty`) |
| `AI WORKFLOW EXECUTE` | `AI WORKFLOW EXECUTE nome` | Esegue workflow (sequenza, vedi §3) |
| `AI.WorkflowList()` | `l$ = AI.WorkflowList()` | Lista nomi workflow (csv) |
| `AI.WorkflowStatus(name$)` | `s$ = AI.WorkflowStatus("W")` | Stato: `""` (in definizione) / `ready` / `empty` / `running` / `completed` / `failed` |
| `AI.WorkflowSteps(name$)` | `p$ = AI.WorkflowSteps("W")` | Nomi passi del workflow (csv) |
| Parametro | Tipo | Default | Semantica |
|---|---|---|---|
| `retry` | int | `0` | Tentativi aggiuntivi dopo il primo fallimento. Totale tentativi = `retry+1` (es. `2` → 3 tentativi). |
| `timeoutMs` | int | `0` | `0` = nessun limite; `>0` = se `onStep` supera `timeoutMs` → `TimeoutException` e retry. Se esauriti → `failed`. |

## 2. Ciclo di vita

IMPORT AI

SUB Main()
    ' 1) Definisci workflow di 3 passi (con retry/timeout)
    AI WORKFLOW PipelineRetry
    AI WORKFLOW STEP estrai, "EXTRACT db", 2, 0        ' 2 retry, nessun timeout
    AI WORKFLOW STEP trasforma, "TRANSFORM csv"        ' nessun retry/timeout
    AI WORKFLOW STEP carica, "LOAD warehouse", 1, 500  ' 1 retry + timeout 500ms
    END WORKFLOW
    PRINT "Passi: "; AI.WorkflowSteps("PipelineRetry") ' estrai,trasforma,carica

    ' 2) Esegui
    AI WORKFLOW EXECUTE PipelineRetry
    PRINT "Stato: "; AI.WorkflowStatus("PipelineRetry") ' completed o failed

    ' 3) Solo retry (3-arg form) e solo timeout


## 3. Semantica retry/timeout

for each step in workflow.Steps
    attempts = 0, max = retry+1
    loop attempt 0..max-1
        try
            if timeoutMs>0  → onStep via Task.Wait(timeoutMs) else onStep diretto
            log "[i] name: action (retry=X, timeout=Yms, attempts=N)" → success → break
        catch TimeoutException / Exception
            log "[i] name: TIMEOUT/ERROR attempt N/M — msg"
            if last attempt → status="failed", log "[workflow 'W'] FAILED at step [i] ..."
                            → return false
            else → ritenta
    → completed solo se tutti i passi passano


// C# — step che fallisce 2× poi passa (retry=2 → 3 tentativi)
int calls = 0;
WorkflowServer.Instance.Execute("W", (name, action) => {
    if (++calls <= 2) throw new Exception("fail");
});


## 4. Esempi — retry e timeout

### 4.1 Retry che recupera

AI WORKFLOW W
AI WORKFLOW STEP s1, "fragile", 2      ' 3 tentativi totali
AI WORKFLOW STEP s2, "stabile"
END WORKFLOW
AI WORKFLOW EXECUTE W   ' in BASIC → completed


### 4.2 Timeout con retry

AI WORKFLOW W
AI WORKFLOW STEP slow, "sleep", 1, 100  ' 1 retry + timeout 100ms
END WORKFLOW


### 4.3 Combinazione

AI WORKFLOW Pipeline
AI WORKFLOW STEP estrai, "EXTRACT", 2, 0
AI WORKFLOW STEP carica, "LOAD", 1, 500
END WORKFLOW


## 5. Funzioni di interrogazione

DIM list$ = AI.WorkflowList()              ' "W1,W2,W3"
DIM st$ = AI.WorkflowStatus("Pipeline")    ' "ready" / "completed" / "failed"
DIM steps$ = AI.WorkflowSteps("Pipeline")  ' "estrai,trasforma,carica"


## 6. Backend e parità
| Backend | Parser | Compiler | Runtime | Verifica |
|---|---|---|---|---|
| **IL** | `Parser.Stmt.Workflow.cs:16` `STEP s, a [, retry [, timeout]]` | `IlProgramCompiler.Statements.cs:679` 4-arg `IlRuntime.WorkflowStep` | `IlRuntime.cs:12820` → `WorkflowServer.AddStep` 4-arg | `WorkflowRetryTimeout.bas` IL → `completed` |
| **PCodeVM** | stesso parser | `Compiler.Statements.cs:994` syscall 4 (`timeout,retry,action,name`) | `PCodeProcessor.Syscalls.cs:2065` pop 4 → `AddStep` | stesso sample PCode → `completed` |
| **VM/C** | — | chunk `OP_SYSCALL WORKFLOW_STEP` argc 4 | `gmb_vm_core.c:2209` `vm_sys_workflow_103` + `gmb_runtime_c/src/gmb_workflow.c:9` `gmb_workflow_step_ex` | `WorkflowRetryTimeout.bas` via `gmb_vm_host` → `completed` |
| **C99** | — | `C99CodeEmitter.cs:3957` → `gmb_workflow_step_ex` | `gmb_workflow.c` fields `retry`/`timeoutMs` | transpiled C → `completed` |
| **C99Native** | `gmb_parser_stmt_workflow.c` (todo) | `gmb_c99_emit.c` (todo) | `gmb_workflow.c` | parità via `gmb_workflow_step`/`_ex` |

## 7. Errori comuni

## 8. Collegamenti

## 9. Roadmap (ancora TODO)

### WorkflowDag-Guide

*Fonte: `WorkflowDag-Guide`*


## 1. WorkflowEngine (B4.1)
| Nodo | Metodo | Semantica |
|---|---|---|
| Step | `AddStep(name, action [, retry [, timeout]])` | singola azione |
| Parallel | `BeginParallel("P") ... EndGroup()` | figli eseguiti in **parallelo** (`Task`), `[PARALLEL P]` nel log |
| Branch | `BeginBranch("BR") ... EndGroup()` | figli candidati; il **resolver** sceglie l'indice (altri skippati), `[BRANCH BR] scelto #n` nel log |

var e = WorkflowEngine.Instance;
e.BeginDag("WB");
e.AddStep("A", "actA");
e.BeginBranch("BR");
e.AddStep("B", "actB");
e.AddStep("C", "actC");
e.EndGroup();
e.AddStep("D", "actD");
e.EndDag();

var (ok, visited, log) = e.Execute("WB",
    onStep: (n, a) => Console.WriteLine($"run {n}"),
    onBranch: (bid, act, childCount) => 1);   // sceglie C
// visited: A, BR, C, D  —  B non eseguito


## 2. Broadcast / Redefine (B4.2, già L1)

## 3. Verifica (B4.3)

dotnet test --filter WorkflowEngineTests --verbosity minimal  # 4/4
dotnet test --filter "Category=Fast" --verbosity minimal      # 439/439


## 4. Riferimenti

<!-- END SURFACE -->








<!-- BEGIN DOMAIN TABLE -->
## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **AGENT.RUN(x1)** — `ag = AGENT.RUN("test", 1)` (shared/samples/cua/CuaDiagnostics.bas:50)
<!-- END DOMAIN TABLE -->
