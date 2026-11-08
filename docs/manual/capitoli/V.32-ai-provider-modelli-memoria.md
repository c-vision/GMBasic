# V.32 — AI: provider, modelli e memoria

## Cosa ottieni da questo capitolo

La superficie `AI`: registrare un provider, scegliere il modello attivo, mandare un prompt, gestire
la memoria della conversazione, usare OCR/audio/immagini e prendere **decisioni tipizzate** con
`AI DECIDE`. È il capitolo che rende il resto della Parte V utilizzabile.

## 1. Le capacità

L'AI è organizzata per **capacità**, non per fornitore. Le costanti del linguaggio sono `%CHAT`,
`%OCR`, `%AUDIO`, `%EMBED` e simili: si dichiara *cosa* serve, poi si dice *chi* lo fornisce.

## 2. Registrare un provider

```basic
AI REGISTER "locale", %CHAT, "~/ai/Models/Qwen", "{""engine"":""mlx"",""swap"":""true""}"
AI SET ACTIVE PROVIDER %CHAT, "locale"
```

`AI REGISTER` associa un nome a una capacità; il quarto argomento è un JSON di opzioni (qui:
motore `mlx`, pesi paginati da disco). `AI SET ACTIVE PROVIDER` dice quale provider usa `%CHAT`.
La lettura corrispondente è una funzione:

```basic
attivo$ = AI.ACTIVE(%CHAT)
```

Comando per scrivere, funzione per leggere: la regola di II.2 vale anche qui.

## 3. Conversare

Un chat ha una **sessione** e una **memoria**: il modello non ricorda nulla fra due chiamate se
non glielo si passa. `AI.MEMORY` gestisce il contesto (aggiungere, riassumere, troncare), e le
guide `AI-Memory-Guide` e `AI-Namespace-Manual` ne descrivono le politiche.

La cosa che conta: la memoria ha un costo lineare nei token. Un agente che accumula tutto il
contesto in una conversazione lunga smette di funzionare a un certo punto, e non per un bug.

## 4. Documenti, immagini, OCR, audio

| Capacità | Cosa fa | Guida |
|---|---|---|
| `%OCR` | testo da un'immagine | `AI-OCR-Audio-Guide` |
| `%AUDIO` / ASR | trascrizione | `ASR-Qwen3-ASR-Guida-Utente` |
| `%CHAT` con visione | descrivere un'immagine | `AI-Immagini-Guide` |
| documenti | estrazione, chunking, indicizzazione | `AI-Documenti-Guide` |

Il pattern è sempre lo stesso: registri il provider per la capacità, poi usi i comandi della
capacità. Non c'è una funzione diversa per ogni fornitore.

## 5. Provider remoti e locali

| Tipo | Vantaggio | Costo |
|---|---|---|
| remoto (API) | nessun modello da scaricare | latenza, costo per token, dati fuori |
| locale (MLX, llama.cpp) | riservatezza, nessun costo per token | RAM/VRAM, download, qualità variabile |

`AI-Providers-Guide` elenca i fornitori supportati. La scelta non è tecnica: è una decisione su
dove finiscono i dati. Se il documento contiene dati personali, il provider locale non è una
preferenza, è un requisito.

## 6. Skills, plugin e MCP

Un agente può avere strumenti esterni: funzioni proprie (`AI-Skills-Plugins-MCP-Guide`), connettori
verso servizi (`Connector-Runtime-Guida-Utente`), server MCP. Ogni strumento è un confine di
sicurezza: ciò che l'agente può chiamare, l'agente può farlo **davvero**. Il capitolo V.33 tratta
il controllo dei permessi.

## 6bis. Decisioni tipizzate: `AI DECIDE` (JEV)

Una decisione non è un testo generato: è **evidenza** — un'opzione scelta, una probabilità, una
confidenza — che il programma usa per decidere. `AI DECIDE` non esegue azioni e non scrive
memoria: restituisce il risultato, e la soglia che lo trasforma in un effetto resta nel codice
applicativo.

### La forma singola

```basic
DIM d AS DECISION
AI DECIDE "Il cliente ha detto sì: è una frode?", "NOUL", "rules" INTO d
PRINT d.OPTION; " "; d.PROBABILITY
```

Il secondo argomento è il **dominio**: `NOUL` (vero/falso), `SCORE` (0..5), `SCORE:0:10`
(rubrica linguistica a 11 livelli), `CHOICE` (16 slot) o `CHOICE:etichetta1|etichetta2|…`
(dominio libero). Il risultato si legge dai membri del tipo `DECISION`.

### La forma batch

Più domande sullo **stesso stato** si scrivono in un blocco. Il batch è una sola esecuzione: le
domande sono pure e indipendenti, e il provider può condividere il prefill.

```basic
DIM frode AS DECISION
DIM gravita AS DECISION

AI DECIDE
    frode AS NOUL
    gravita AS SCORE 0 TO 10
FROM "Il cliente ha detto sì: è una frode?"
USING "rules"
END DECIDE

PRINT frode.OPTION; " "; gravita.VALUE
```

Le destinazioni sono i nomi delle domande. Il blocco accetta `USING` e `FALLBACK` come la forma
singola. Il MiniTest `shared/minitests/ai/ai_decide_batch.bas` è l'oracolo di questa forma.

### Il tipo `DECISION`

`DIM d AS DECISION` crea un valore JSON tipizzato; `DECISION<NOUL>` documenta il dominio atteso.
I membri sono:

| Membro | Tipo | Significato |
|---|---|---|
| `OPTION` | stringa | identificatore dell'opzione scelta (`true`, `A`, `3`) |
| `VALUE` | stringa | valore dell'opzione (`true`/`false`, il numero, l'etichetta) |
| `PROBABILITY` | numero | probabilità dell'opzione scelta, fra 0 e 1 |
| `CONFIDENCE` | numero o vuoto | confidenza calibrata; vuoto senza artefatto |
| `CALIBRATED` | numero o vuoto | probabilità calibrata; vuoto senza artefatto |
| `KIND` | stringa | `NOUL`, `CHOICE` o `SCORE` |

`CONFIDENCE` e `CALIBRATED` non sono mai copie di `PROBABILITY`: restano vuoti finché non esiste
una calibrazione misurata. Un numero inventato sarebbe peggio di nessun numero.

### I provider

Il provider si sceglie con `USING`; senza, vale il ruolo `%DECIDE` attivo.

| Provider | Come decide | Uso tipico |
|---|---|---|
| `rules` | regole deterministiche | test, fallback prod-safe, nessun modello |
| `mock` | opzione fissa | test del solo control flow |
| `local` | legge i logit del modello MLX | modello locale, calibrabile |
| `typesafe` | HTTP verso un servizio che restituisce la decisione già strutturata | JEV remoto |
| `adapter` / `llm` | fa **generare** il modello e riconosce l'opzione | endpoint senza logprob |
| `clm` | HTTP `POST /v1/systemone` verso un server CLM (Contrastive Language Models) | decisione per similarità su embedding |

`local` e `adapter` richiedono un modello attivo; `typesafe` e `clm` una configurazione `baseUrl`;
`rules` e `mock` non richiedono nulla e sono gli unici usabili in un test senza rete.

#### Il provider `clm`

CLM non è una primitiva nuova: è un **provider** di `AI DECIDE`, con lo stesso contratto
(`NOUL`/`CHOICE`/`SCORE`). La differenza è nel modo di decidere: non legge i logit delle opzioni, ma
confronta l'embedding dello stato con quello di ogni candidato (`softmax(scala · coseno)`). La
`PROBABILITY` che restituisce è quindi una **distribuzione sulle opzioni**, non una correttezza
calibrata: `CALIBRATED` resta `null`.

```basic
AI REGISTER "team", %DECIDE, "clm", "{""baseUrl"":""http://127.0.0.1:8700"",""apiKey"":""…""}"
AI DECIDE "{""ticket"":""Login fallito dopo il reset""}", "CHOICE:auth|billing", "team" INTO r
```

- **Configurazione** (`AI REGISTER`): `baseUrl` (se manca `/v1/systemone` viene aggiunto), `apiKey`
  (inviata come `Authorization: Bearer`), `timeoutMs`, `instructions` (testo della domanda, opzionale).
- Lo **stato può essere un oggetto JSON**: viene inviato come JSON e il server lo rende in prosa
  (`chiave: valore`); le head CLM sono addestrate su prosa, non su JSON.
- Ogni errore è **esplicito** (`{"error":"clm: …"}`): config senza `baseUrl`, HTTP non 2xx, risposta
  di tipo diverso da quello chiesto, opzione fuori dal dominio, `probabilities` assenti. Non c'è mai
  un fallback silenzioso: se lo vuoi, scrivilo (`FALLBACK "rules"`).
- **Percorso locale (senza server, senza CUDA)**: al posto di `baseUrl` si danno `head` (i pesi della
  head, in `safetensors`) ed `encoder` (la cartella HF di **Qwen3-8B**, in bf16: la head è addestrata
  su quell'encoder, e con un altro encoder o con una versione quantizzata i risultati non sono garantiti). L'encoder
  gira su MLX con il pooling sull'**ultimo token** (dopo la RMSNorm finale); la head e il punteggio
  `softmax(scala · coseno)` sono in C. I testi oltre 2048 token si troncano tenendo gli ultimi 2048,
  come fa il server.

  ```basic
  AI REGISTER "clm", %DECIDE, "clm", "{""head"":""~/ai/Models/CLM-v0.1-8B/clm_head.safetensors"",""encoder"":""~/ai/Models/Qwen3-8B""}"
  AI DECIDE "What causes tides on Earth?", "CHOICE:The Moon's gravitational pull.|Photosynthesis in plants.|Because the Earth is round.", "clm" INTO r
  ```

  Qui le opzioni di `CHOICE` sono il **testo dei candidati**: con CLM scegli fra frasi libere, e il
  risultato non dipende dall'ordine in cui le elenchi. La domanda (se c'è) va nel config come
  `instructions`. Il sample `shared/samples/AiLLM/CLMLocal.bas` mostra l'uso completo.
- **Preparare i pesi**: il checkpoint pubblicato è un file `torch.save` (`CLM_v0.1-8B.pt`, 75 MB). Si
  converte una volta con lo strumento C del repo, senza Python e senza eseguire il file (il lettore
  accetta solo un sottoinsieme ristretto del formato e rifiuta tutto il resto):

  ```bash
  cc -O2 tools/c/clm_head_convert.c -o clm_head_convert -lm
  ./clm_head_convert ~/ai/Models/CLM-v0.1-8B/CLM_v0.1-8B.pt ~/ai/Models/CLM-v0.1-8B/clm_head.safetensors
  ```

  Produce `clm_head.safetensors` e il file gemello `clm_head.json` (attivazione, residual, dimensioni).
- **Dalle altre gambe**: l'implementazione è una sola, nel runtime C (VM/C e C99Native la usano
  direttamente; IL e PCode la chiamano via P/Invoke dalla libreria condivisa `libgmbclm`). Le quattro
  gambe danno lo stesso risultato.
- **`RANK`: scegliere fra candidati liberi.** È un *tipo* di `AI DECIDE`, non una sintassi nuova:
  `"RANK:cand1|cand2|…"` (almeno due candidati, senza il limite di 16 opzioni di `CHOICE`) e restituisce i
  candidati **ordinati per probabilità**. Vale solo col provider `clm`; con gli altri dà un errore esplicito.

  ```basic
  AI DECIDE "What causes tides on Earth?", "RANK:Because the Earth is round.|Photosynthesis in plants.|The Moon's gravitational pull.", "clm" INTO r
  ' {"kind":"RANK","ranked":[{"rank":1,"candidate":"The Moon's gravitational pull.","probability":0.993527},…],"confidence":0.990290}
  ```

  Come i candidati non possono contenere `|`. In locale ogni candidato costa un passaggio dell'encoder.
- **`AI DECIDE VERIFY` con `clm`.** Confronta provider diversi sulla stessa richiesta (accordo, deriva,
  scarto di probabilità); per ogni provider `clm` aggiunge le **misure** dell'esecuzione: `mode`
  (`local`/`remote`), `ms` totali, `encoderMs`, `headMs`, `netMs`, `embeddings` (quanti testi sono passati
  dall'encoder: non c'è cache, `"cache":"off"`) e `margin` (probabilità del primo meno quella del secondo).
- **Quanto è sensibile**: la scala della head è 100, quindi differenze minime nel coseno spostano molto le
  probabilità. L'esempio di ranking del README di CLM si riproduce (0,9935 contro 0,993 dichiarato). Sull'esempio
  del ticket otteniamo 0,988 contro 0,939 dichiarato (stessa scelta, `billing`): la precisione di calcolo
  **non** è la causa (fp32 e bf16 danno 0,98792 e 0,98788), il testo dell'input coincide con il sorgente del
  server, e il valore 0,939 compare solo nel README, non nel codice o nei suoi test. Trattale come stime, non
  come valori da confrontare al decimale con un'altra installazione.

Il ruolo di decisione si registra con la costante `%DECIDE`:

```basic
AI REGISTER "giudice", %DECIDE, "rules"
AI SET ACTIVE %DECIDE, "giudice"
```

Da quel momento `AI DECIDE` senza `USING` usa `giudice`. La catena di fallback è **esplicita**:

```basic
AI DECIDE "sì", "NOUL", USING "local" FALLBACK "rules","mock" INTO d
```

Vince il primo provider che non risponde con un errore; se falliscono tutti, il risultato è
`{"error":"tutti i provider falliti: …"}`. Non esiste un fallback implicito.

### Diagnostica: `AI DECIDE VERIFY`

`VERIFY` esegue la stessa richiesta su più provider e riporta accordo e scarto. Non modifica lo
stato.

```basic
AI DECIDE VERIFY "sì", "NOUL", "rules|mock" INTO rapporto$
PRINT rapporto$
```

Il report contiene `ok`, `agreement`, `argmaxDrift`, `probabilitySpread` e l'elenco dei risultati
per provider. Serve a vedere **dove** due provider divergono, non a nasconderlo.

### Configurazione per modello

Le chiavi di decisione stanno nella configurazione del provider di chat, dove vivono già
`modelDir` e `engine`:

```basic
AI REGISTER "locale", %CHAT, "~/ai/Models/Qwen", "{""engine"":""mlx"",""decide"":""auto""}"
```

| Chiave | Valori | Effetto |
|---|---|---|
| `decide` | `off`, `on`, `auto` | `off` disabilita la decisione per quel modello con errore esplicito |
| `decideTemplate` | testo | se contiene `{state}`/`{options}` è il messaggio utente; altrimenti è il testo di sistema |
| `decideCalibration` | percorso | artefatto `scoring_head.json` esplicito; assente o illeggibile = errore |

La calibrazione predefinita è `<cartella modello>/scoring_head.json`. L'artefatto contiene i bias
per slot, la temperatura globale e quelle per tipo; il readout li applica prima della softmax.
Senza artefatto `CALIBRATED` resta vuoto. Gli strumenti di calibrazione e valutazione sono nel
perimetro PUL (`GMBasic/gmbllm/tools/`); l'inferenza resta nel perimetro Apache di LLMEnv.

### Policy: la soglia sta nel programma

`AI DECIDE` produce evidenza; l'effetto è una decisione del programma. Il pattern è una
condizione esplicita:

```basic
IF frode.VALUE = "true" AND frode.PROBABILITY >= 0.8 THEN
    PRINT "policy: blocco"
ELSE
    PRINT "policy: nessuna azione"
END IF
```

Il sample completo è `shared/samples/Ai/DecidePolicy.bas`; l'oracolo è
`shared/minitests/ai/ai_decide_policy.bas`. La stessa regola vale per la memoria: il verdetto è
evidenza, la promozione o la cancellazione resta una policy tracciata.

### Parità fra i backend

Le forme singola e batch, `DECISION<T>`, `%DECIDE`, `FALLBACK`, `VERIFY`, `local`, `typesafe` e
`adapter` sono verificate sui **quattro backend**: IL, PCodeVM, VM/C, C99Native (`parity4`). I
MiniTest `shared/minitests/ai/ai_decide_*.bas` sono gli oracoli; il provider `rules` li rende
deterministici e senza modello.

### Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| `local provider non disponibile: nessun modello MLX attivo` | `USING "local"` senza modello attivo | registra e attiva un modello, oppure usa `rules`/`mock` |
| `l'opzione '…' non è un token singolo` | etichetta `CHOICE` multi-token | usa etichette single-token o il verbalizer A..P |
| `adapter: la risposta non corrisponde a un'opzione` | generazione fuori dominio | stringi `decideTemplate` o usa `local` |
| `decisione disabilitata per questo modello (decide=off)` | chiave `decide` = `off` | cambia la configurazione del provider |
| `{"error":"tutti i provider falliti: …"}` | catena di fallback esaurita | controlla i provider e i loro errori |

### Dove andare adesso

V.33 mostra come un agente usa una decisione per il routing; V.35 descrive il modello locale e il
suo costo; III.12 spiega il confine fra runtime e inferenza. Per la memoria, il verdetto
`STRONG|WEAK|NONE` è descritto in V.32 e nel workstream memoria.

## 7. Perché questo capitolo non ha un output

Gli esempi di conversazione richiedono un provider attivo — una chiave API o un modello
scaricato. In questo ambiente non c'è nessuno dei due, e inventare un output sarebbe la cosa
peggiore da fare in un manuale. Gli esempi di `AI DECIDE` con il provider `rules`, invece, sono
deterministici e **sono eseguiti**: `shared/samples/Ai/DecidePolicy.bas` e i MiniTest
`shared/minitests/ai/ai_decide_*.bas` riportano l'output reale. Per il resto, il codice citato è
quello canonico del linguaggio: trattalo come sintassi, non come risultato.

## 8. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| `AI.ACTIVE(%CHAT)` vuoto | nessun provider attivo per la capacità | `AI SET ACTIVE PROVIDER` |
| il modello non ricorda | memoria non passata | aggiungi il contesto alla sessione |
| risposta troncata | limite di token in uscita | alza il limite o accorcia il prompt |
| costo che esplode | contesto che cresce | riassumi, tronca, usa RAG |
| dati che escono | provider remoto | passa a un provider locale |

## 9. Dove andare adesso

- **V.33** — agenti e workflow sopra questi provider.
- **V.34** — RAG: dare al modello i tuoi documenti.
- **V.35** — un LLM che gira sulla tua macchina.














<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### AI-Namespace-Manual

*Fonte: `AI-Namespace-Manual`*


## 1. Mappa — dove andare
| Bisogno | Vai a | Cosa trovi |
|---|---|---|
| Registrare e attivare modelli (locale/remoto, `%CHAT`/`%OCR`/`%AUDIO`, discovery, credit routing) | [[Guide/utente/AI-Providers-Guide]] | `AI REGISTER`, `AI SET ACTIVE/KEY/TEMP/BASE`, `AI DISCOVER`, `AI SAVE CONFIG`, esempi `llamacpp`/`mlx`/`openai`/`ollama` |
| Modelli locali su disco (`AIPATH`, `hf:`) | [[Guide/utente/GMBasic-Modelli-AI-Locali]] | `config/ai.json` → `modelPaths`, scan `~/ai/Models` |
| Self-distillation lm_head-only (`AI DISTILL`) | [[Guide/utente/AI-Distill-Guida-Utente]] | `AI DISTILL BUILD/SAMPLE/EVAL/TRAIN/APPLY/RESTORE` + `LLM.DISTILL*()` — Stream D |
| Agenti (ciclo di vita, `SET`, `ASK`, streaming, tool, thinking) | [[Guide/utente/Agents-Manual]] | `AI AGENT CREATE/SET/ASK STREAM INTO`, `AI.AGENTASKSYNC`, proprietà `thinking`/`reflection` |
| Workflow (orchestrazione passi, retry/timeout) | [[Guide/Workflow-Guide]] | `AI WORKFLOW / STEP s, act [, retry [, timeout]] / EXECUTE`, `AI.Workflow*()` — nuovo 2026-09-07 |
| Team RecursiveMAS (ricorsione latente, `RECURSION`/`LATENT`/`LINK`, training) | [[Guide/utente/RecursiveMAS-Manual]] | `AI TEAM CREATE/SET RECURSION/LATENT/LINK/THOUGHTS`, `TRAIN INNER/OUTER`, `AI.TEAMLIST` |
| Vettori e RAG (collezioni SQLite+FAISS, `EMBED`/`SEARCH`/`ASK`) | [[Guide/utente/AI-RAG-Guide]] | `AI RAG ENSURE/INDEX/LOAD`, `AI.RAGCONTEXT/ASK` |
| Memoria e Second Brain (`brain.db`, `WATCH`/`IMPORT`) | [[Guide/utente/AI-Memory-Guide]] | `AI MEMORY NOTE/WATCH/REFRESH`, `AI.MEMORYASK` |
| OCR e trascrizione (`PaddleOCR-VL`, `Qwen2.5-Omni`, `apple-vision`) | [[Guide/utente/AI-OCR-Audio-Guide]] | `AI.OCR`, `AI.TRANSCRIBE`, `AI.ANALYZEIMAGE`, redact PII |
| Skill, plugin operativi e MCP | [[Guide/utente/AI-Skills-Plugins-MCP-Guide]] | `AI SKILLS/PLUGINS ADD/ENABLE/SET INDEX`, `AI.PLUGINCALL/PLUGINSINFO/PLUGINSLIST/PLUGINLIST` |
| Browser headless + `PROCESS`/`BROWSEREXTRACT` | [[Guide/utente/Browser-Automation-Plugin]] | `gmb setup BROWSER`, `plugins/browser/browser.bas`, `AI PROCESS`, `AI.BROWSEREXTRACT` |
| Connettori (52, Gmail/Drive/GitHub/Slack/DB/HTTP/FS) | [[Guide/Connector-Runtime-Guida-Utente]] | `AI CONNECTOR LOAD/REGISTER/CALL`, `AI.CONNECTORSEARCH/CALL`, `52` T0-T4, OAuth/PKCE, `policy` `HIGH_RISK` |
| Marketplace HF dei team | `§9 di Agents-Manual` + `AI SET HFTOKEN/HFREPO` | `AI TEAM SAVE/LOAD`, `config/recursivemas/` |

## 2. Riferimento rapido (completo, ma vai alle guide per i dettagli)
| Comando / Funzione | Sintassi | Dove approfondire |
|---|---|---|
| `AI REGISTER` | `AI REGISTER alias, capability, source [, param]` — `param` può contenere `{"engine":"mlx"}`; un `.gguf` con arch in `ARCH_TABLE` (DS4/`deepseek4`) è auto-routato al **motore MLX embedded** | Providers-Guide §2.2bis |
| `AI SET ACTIVE` | `AI SET ACTIVE cap, nome` (cap = `%CHAT` o `"completion"`) | Providers-Guide |
| `AI DISCOVER` | `AI DISCOVER pfx, cap, url, key [, soloFree]` | Providers-Guide |
| `AI RAG ...` | `AI RAG INDEX TEXT db, coll, id, testo` | Vector-RAG-Guide |
| `AI.EMBED/SEARCH/ASK` | `r$ = AI.SEARCH(db, coll, "query", 5)` | Vector-RAG-Guide |
| `AI.KGADDNODE/ADDEDGE/QUERY` | `AI.KGQUERY("path a->b")` (grafo in-memory, no LLM) | §2.5 sotto |
| `AI MEMORY ...` | `AI MEMORY NOTE testo` | Memory-Guide |
| `AI.OCR` / `AI.TRANSCRIBE` | `t$ = AI.OCR("scan.png")` | OCR-Audio-Guide |
| `AI SKILLS/PLUGINS` | `AI PLUGINS ENABLE "web-fetch"` | Skills-Plugins-MCP-Guide |
| `AI MCP ...` | `AI MCP SERVER ADD "fs", "npx", ...` | Skills-Plugins-MCP-Guide |
| `AI AGENT ...` | `AI AGENT CREATE nome` | Agents-Manual |
| `AI WORKFLOW ...` | `AI WORKFLOW STEP s, act [, retry [, timeout]]` | Workflow-Guide (retry/timeout 2026-09-07) |
| `AI VERIFY` | `AI VERIFY <attempt> REQUIRE <requireExpr> [INTO var]` | §2.6 sotto (verifier/retry col REQUIRE su EVAL) |
| `AI ASKCHAIN` | `AI ASKCHAIN <nome>, <prompt>, <prefix> [INTO var]` | §2.8 sotto (ragionamento concatenato via Blackboard) |
| `AI EVAL VALIDATE` | `AI EVAL VALIDATE <result> AGAINST [VAULT] <folder> [INTO var]` | §2.9 sotto (UAT automatica via vault) |
| `AI TEAM ...` | `AI TEAM CREATE Squadra` | RecursiveMAS-Manual |
| `AI CONNECTOR ...` | `AI CONNECTOR LOAD "gmail"` / `CALL "gmail","search",...` | §2.10 + Connector-Guide |
| `AI PROCESS` | `AI PROCESS file PROMPT istruzioni` | Browser-Plugin (§5) |
| `AI.BROWSEREXTRACT` | `j$ = AI.BROWSEREXTRACT(url, prompt, schema)` | Browser-Plugin (§5) |
| `AI SET HFTOKEN/HFREPO` | `AI SET HFREPO repo` | RecursiveMAS-Manual |
| `AI FINETUNE` | `AI FINETUNE "dir", { "dataset":…, "ckpt":… }` | §2.7 sotto (fine-tune LoRA MLX, superficie F1→F3) |
| `AI LORA APPLY` | `AI LORA APPLY "ckpt", { "model":…, "dataset":… }` | §2.7 sotto (deploy/verifica adapter) |
| `AI.CATEGORY*` | `AI.CATEGORYSET "ultrabrain","gpt-frontier"` | §2.11 sotto (categorie di task → provider) |
| `AI.TASK*` | `AI.TASKDEFINE "a","step"` / `AI.TASKRUN()` | §2.11 sotto (Task Graph con dipendenze) |
| `AI.PLAN*` | `AI.PLANDEFINE "p1","goal"` / `AI.PLANRUN("p1")` | §2.11 sotto (Planner + approvazione) |
| `AI.HOOK*` | `AI.HOOKREGISTER "before_task","Sub"` | §2.11 sotto (lifecycle hooks) |
| `AI.ARTIFACT*` | `AI.ARTIFACTPUT "nome","path"` | §2.11 sotto (Artifact Store) |
| `AI.RESOURCE*` | `AI.RESOURCESET "max_agents","4"` | §2.11 sotto (Resource Manager) |
| `AI.HANDOFF*` | `AI.HANDOFF "t1","agentA"` | §2.11 sotto (Handoff) |
| `AI.RULES*` | `AI.RULESADD "/proj/src","regole"` | §2.11 sotto (Rules injection gerarchica) |
| `AI.GOAL*` | `AI.GOALSET "obiettivo"` / `AI.GOALTICK()` | §2.11 sotto (Goal + idle continuation) |
| `AI.REPLAY*` | `AI.REPLAYSTART "s1"` / `AI.REPLAYGET("s1")` | §2.11 sotto (Session Replay) |
| `AI.METRIC*/FLEETSTATS` | `AI.METRICRECORD "a","tokens",100` | §2.11 sotto (metriche fleet-level) |
| `AI.HASHLINE/HASHEDIT/HASHVERIFY` | `AI.HASHLINE(content$)` / `AI.HASHEDIT(c$,"2","nuovo")` | §2.11 sotto (edit ancorato) |
| `AI.CHECKPOINT/RESUME/CHECKPOINTLIST/CLEAR` | `AI.CHECKPOINT "s1","step=3"` / `AI.RESUME("s1")` | §2.11 sotto (checkpoint/recovery) |
| `AI.ORCHSAVE/ORCHLOAD` | `AI.ORCHSAVE "config/orchestration.db"` | §2.11 sotto (persistenza SQLite) |

### 2.5 `AI.KG*` — Knowledge Graph in-memory (B3.5, 2026-09-07)
| Funzione | Sintassi | Ritorna |
|---|---|---|
| `AI.KGADDNODE(nome)` | crea il nodo | `1` (idempotente) |
| `AI.KGADDEDGE(a, b)` | crea a→b (crea i nodi se mancano) | `1` (idempotente, no duplicati) |
| `AI.KGQUERY(nodo)` | vicini + archi incidenti | stringa JSON |
| `AI.KGQUERY("path a->b")` | cammino minimo | stringa JSON |

AI.KGADDNODE("GMBasic") : AI.KGADDNODE("Parser")
AI.KGADDEDGE("GMBasic", "Parser")
PRINT AI.KGQUERY("Parser")           ' {"node":"Parser","neighbors":[...],"incident":[...]}
PRINT AI.KGQUERY("path GMBasic->Parser")   ' {"from":...,"to":...,"path":[...]}


### 2.6 `AI VERIFY ... REQUIRE` — verifier/retry col REQUIRE valutato sul risultato (2026-09-09)
| Sintassi | Significato |
|---|---|
| `AI VERIFY <attempt> REQUIRE <requireExpr>` | verifica e scarta l'esito (statement puro) |
| `AI VERIFY <attempt> REQUIRE <requireExpr> INTO <var$>` | scrive il risultato finale accettato in `var$` |

IMPORT AI WITH ALIAS
SUB Main()
  DIM esito AS STRING
  DIM soglia AS INTEGER
  soglia = 8
  AI VERIFY "COMPILE /percorso/Programma.bas" REQUIRE "LEN(RESULT$) > soglia" INTO esito
  PRINT "ok:"; esito          ' "Compile OK"
END SUB


### 2.7 `LLM FINETUNE` / `LLM LORA APPLY` — fine-tune LoRA su modelli MLX locali

AI FINETUNE "/Users/gaetano/ai/Models/Llama-3.2-1B-Instruct-4bit", _
            { "dataset" = "../../Fixtures/lora_rule.train.bin", "holdout" = "../../Fixtures/lora_rule.hold.bin",
              "ckpt" = "work/ft1", "rank" = 4, "layer" = 15, "steps" = 60,
              "batch" = 4, "lr" = 0.01, "seed" = 7, "verify" = TRUE }
(le chiavi del dict vanno tra virgolette: chiavi nude multiple = bug parser)

| Funzione | Ritorna |
|---|---|
| `AI.FINETUNEDONE()` | `-1` se l'ultima run è terminata con successo |
| `AI.FINETUNELOSS()` | ultima loss media train |
| `AI.FINETUNEHOLD()` | ultima loss holdout |
| `AI.FINETUNESTATUS()` | stringa di stato (idle/running/ok/error) |
| `AI.FINETUNECKPT()` | path del checkpoint salvato |
| `AI.FINETUNEGREEDY()` | token greedy prima dell'addestramento (negativo = n/d) |
| `AI.FINETUNEGREEDYAFTER()` | token greedy dopo l'addestramento (negativo = n/d) — certifica il cambio |
| `AI.FINETUNEDEMOCHANGED()` | `1` se la demo cambia dopo il fine-tune, `0` altrimenti |
| `AI.FINETUNEBASETRAIN()` | loss train del baseline (pre-fine-tune) — rende interpretabile il −X% |
| `AI.FINETUNEBASEHOLD()` | loss holdout del baseline (pre-fine-tune) |

IMPORT AI
SUB Main()
    AI FINETUNE "dir", { "dataset" = "work/rule.train.bin", "ckpt" = "work/ft1", "steps" = 60 }
    IF AI.FINETUNEDONE() = -1 THEN
        PRINT "train loss:"; AI.FINETUNELOSS(); " (baseline:"; AI.FINETUNEBASETRAIN(); ")"
        PRINT "hold:"; AI.FINETUNEHOLD(); " (baseline:"; AI.FINETUNEBASEHOLD(); ")"
        PRINT "greedy base:"; AI.FINETUNEGREEDY(); " after:"; AI.FINETUNEGREEDYAFTER()
        PRINT "demo changed:"; AI.FINETUNEDEMOCHANGED()
    ELSE
        PRINT "errore:"; AI.FINETUNESTATUS()
    END IF
END SUB


AI LORA APPLY "work/ft1", { "model": "/…/Llama-3.2-1B-Instruct-4bit",
                           "dataset": "../../Fixtures/lora_rule.train.bin" }
PRINT AI.FINETUNESTATUS()   ' "ok: ... l'adapter ricaricato cambia l'inferenza"


### 2.8 `AI ASKCHAIN` — ragionamento concatenato (Blackboard-as-Artifacts) (2026-09-09)
| Sintassi | Significato |
|---|---|
| `AI ASKCHAIN <nome>, <prompt>, <prefix>` | anello di ragionamento, esito scartato |
| `AI ASKCHAIN <nome>, <prompt>, <prefix> INTO <var$>` | scrive la risposta dell'anello in `var$` |

IMPORT AI WITH ALIAS
AI AGENT CREATE "Ragionatore"
DIM esito AS STRING
AI ASKCHAIN "Ragionatore", "Analizza il problema in 3 punti", "catena" INTO esito
AI ASKCHAIN "Ragionatore", "Sintetizza una soluzione", "catena" INTO esito   ' riusa lo stato


### 2.9 `AI EVAL VALIDATE` — UAT automatica via vault (2026-09-09)
| Sintassi | Significato |
|---|---|
| `AI EVAL VALIDATE <result> AGAINST [VAULT] <folder>` | valida, esito scartato |
| `AI EVAL VALIDATE <result> AGAINST VAULT <folder> INTO <var$>` | scrive il verdetto JSON in `var$` |

IMPORT AI WITH ALIAS
DIM json AS STRING
AI EVAL VALIDATE "il sistema supporta il login e calcola il totale" AGAINST VAULT "/percorso/requisiti" INTO json
PRINT json     ' {"ok":true,...}


### 2.9bis `AI DECIDE` — decision layer JEV/SystemOne (v0, 2026-11-02)
| Sintassi | Significato |
|---|---|
| `AI DECIDE <state>, "NOUL" [INTO <var$>]` | decisione booleana `false`/`true` |
| `AI DECIDE <state>, "SCORE" [INTO <var$>]` | score `0..5` |
| `AI DECIDE <state>, "CHOICE" [INTO <var$>]` | scelta negli slot `A..P` |
| `AI DECIDE <state>, "CHOICE:label1|label2|..." [INTO <var$>]` | dominio libero: le label mappano gli slot `A..P` (max 16); il JSON riporta `value` = label |
| `AI DECIDE <state>, "<KIND>", "rules"|"mock" [INTO <var$>]` | provider esplicito (v0: `rules` deterministico, `mock` prima opzione a 1.0) |

IMPORT AI
DIM r AS STRING
AI DECIDE "il cliente ha detto SÌ", "NOUL" INTO r
PRINT r     ' {"kind":"NOUL","option":"true",...}
AI DECIDE "x", "NOUL", "mock" INTO r      ' provider esplicito

' tipo DECISION (alias JSON): INTO avvolge automaticamente in JSON.PARSE
DIM d AS DECISION
AI DECIDE "il cliente ha detto SÌ", "NOUL" INTO d
PRINT d.VALUE          ' true
PRINT d.PROBABILITY    '  1
PRINT d.CONFIDENCE     ' (vuoto = null)

' forma parametrica (parametro documentale: il valore resta l'oggetto JSON)
DIM g AS DECISION<NOUL>
AI DECIDE "il cliente ha detto SÌ", "NOUL" INTO g


### 2.10 `AI CONNECTOR` — Connector Runtime (52 connettori, 8 primitive)
| Sotto | Sintassi | Note |
|---|---|---|
| `AI CONNECTOR LIST` | `AI CONNECTOR LIST` / `AI.CONNECTORLIST()` | Lista registrati (`config/connectors.json`) |
| `AI CONNECTOR REGISTER` | `AI CONNECTOR REGISTER "my-github","github"` | Registra custom (`config/connectors.json`) |
| `AI CONNECTOR LOAD` | `AI CONNECTOR LOAD "gmail"` | Dal marketplace 52 (T0-T4, `ConnectorMarketplace.cs:162`) → auto-registra `gmail.search` in `McpToolRegistry` |
| `AI CONNECTOR SEARCH` | `AI CONNECTOR SEARCH "mail"` / `AI.CONNECTORSEARCH("mail")` | Discovery per capability (`mail` → `gmail`, `db` → `postgres/sqlite`) |
| `AI CONNECTOR CALL` | `AI CONNECTOR CALL "gmail","search","{""query"":""...""}"` / `AI.CONNECTORCALL("gmail","search",...)` | Chiama tool (`ConnectorRuntime.cs:116`, `filesystem`/`http`/`sqlite` reali F1, `gmail`/`github` con `Bearer` se `AI CONNECTOR AUTH`) |
| `AI CONNECTOR POLICY` | `AI CONNECTOR POLICY SET "gmail","send","approve"` | `READ/WRITE/DESTRUCTIVE` + `HIGH_RISK` (`mail.send` → `approve`) |
| `AI CONNECTOR SECRET/AUTH` | `AI CONNECTOR SECRET SET "gmail","api_key","...` / `AUTH "gmail","{""access_token"":""...""}"` | `Keychain`/`Secret Service` per-connector (`ConnectorConfig.cs:13`), `ConnectorAuth.cs:155` PKCE |
| `AI CONNECTOR MARKETPLACE` | `AI CONNECTOR MARKETPLACE LIST` / `GET "gmail"` | 52 T0 11 + T1 12 + T2 10 + T3 11 + T4 9 |
| `AI CONNECTOR RESOURCES/PROMPTS` | `AI CONNECTOR RESOURCES "gmail"` | MCP `resources/read` `prompts/get` + `ping` (`McpServer.cs:80`) |

IMPORT AI
AI CONNECTOR LOAD "gmail"
AI CONNECTOR LOAD "filesystem"
AI CONNECTOR SECRET SET "gmail", "api_key", "ya29.demo"
AI AGENT CREATE "MailAgent"   ' auto-vede gmail.search + filesystem.list
' Con LLM: AI.AGENTASKSYNC("MailAgent","cerca fatture ACME e lista work/")
' → tool_calls gmail.search → filesystem.list (senza REGISTER esplicito)
DIM r$ AS STRING = AI.CONNECTORCALL("gmail","search","{""query"":""from:ACME""}")
PRINT r$


### 2.11 Orchestrazione agenti — categorie, Task Graph, Planner, Hook, Artifact, Resource, Handoff (2026-09-10)
| Funzione | Sintassi | Ritorna |
|---|---|---|
| `AI.CATEGORYSET(cat, provider)` | associa categoria→provider | 1/0 |
| `AI.CATEGORYFALLBACK(cat, provider)` | provider di fallback | 1/0 |
| `AI.CATEGORYAUTO(flag)` | routing automatico on/off | nuovo stato 1/0 |
| `AI.CATEGORYPICK(cat)` | provider scelto ("" se off/assente) | stringa |
| `AI.CATEGORYLIST()` | JSON `{"auto":bool,"categories":{...}}` | stringa |
| Funzione | Sintassi | Ritorna |
|---|---|---|
| `AI.TASKDEFINE(id, desc)` | definisce un task | 1/0 |
| `AI.TASKDEPENDS(id, dep)` | aggiunge dipendenza | 1/0 |
| `AI.TASKASSIGN(id, agente)` | assegna un agente al task | 1/0 |
| `AI.TASKSTATUS(id)` | stato del task | JSON |
| `AI.TASKLIST()` | tutti i task | JSON |
| `AI.TASKRUN()` | esegue il DAG in ordine topologico | JSON |
| Funzione | Sintassi | Ritorna |
|---|---|---|
| `AI.PLANDEFINE(name, goal)` | crea un piano (draft) | 1/0 |
| `AI.PLANADD(name, taskId)` | associa un task | 1/0 |
| `AI.PLANAPPROVE(name)` | approva il piano | 1/0 |
| `AI.PLANSTATUS(name)` | stato del piano | JSON |
| `AI.PLANLIST()` | tutti i piani | JSON |
| `AI.PLANRUN(name)` | esegue il Task Graph (solo se approvato) | JSON |
| Funzione | Sintassi | Ritorna |
|---|---|---|
| `AI.HOOKREGISTER(event, handler)` | registra un handler | 1/0 (evento validato) |
| `AI.HOOKUNREGISTER(event, handler)` | rimuove un handler | 1/0 |
| `AI.HOOKLIST()` | hook registrati | JSON |
| `AI.HOOKEVENTS()` | elenco eventi noti | JSON |
| `AI.HOOKFIRE(event, payload)` | risolve gli handler dell'evento | JSON |
| Funzione | Sintassi | Ritorna |
|---|---|---|
| `AI.ARTIFACTPUT(name, path)` | registra un file | 1/0 |
| `AI.ARTIFACTLINK(name, uri)` | registra una URI | 1/0 |
| `AI.ARTIFACTGET(name)` | riferimento | stringa |
| `AI.ARTIFACTLIST()` | tutti gli artefatti | JSON |
| `AI.ARTIFACTREMOVE(name)` | rimuove | 1/0 |
| Funzione | Sintassi | Ritorna |
|---|---|---|
| `AI.RESOURCESET(key, value)` | imposta un limite | 1/0 |
| `AI.RESOURCEGET(key)` | valore | stringa |
| `AI.RESOURCELIST()` | tutti i limiti | JSON |
| `AI.RESOURCEAVAILABLE(key)` | disponibile? | 1/0 |
| `AI.RESOURCESWARMSIZE(n)` | min(n, `max_agents`) | numero |
| Funzione | Sintassi | Ritorna |
|---|---|---|
| `AI.HANDOFF(taskId, toAgent)` | registra/aggiorna l'handoff | 1/0 |
| `AI.HANDOFFCOMPLETE(taskId, result)` | marca completato | 1/0 |
| `AI.HANDOFFSTATUS(taskId)` | stato | JSON |
| `AI.HANDOFFLIST()` | tutti gli handoff | JSON |
| `AI.HANDOFFCLEAR()` | svuota | numero rimossi |
| Funzione | Sintassi | Ritorna |
|---|---|---|
| `AI.RULESADD(scope, content)` | registra regole per una directory | 1/0 |
| `AI.RULESFOR(path)` | regole applicabili al path (root→leaf, `\n`-join) | stringa |
| `AI.RULESLIST()` | scope registrati | JSON |
| `AI.RULESREMOVE(scope)` | rimuove uno scope | 1/0 |
| `AI.RULESCLEAR()` | svuota | numero scope rimossi |
| Funzione | Sintassi | Ritorna |
|---|---|---|
| `AI.GOALSET(objective)` | imposta l'obiettivo (attivo) | 1/0 |
| `AI.GOALGET()` | obiettivo corrente | stringa |
| `AI.GOALSTATUS()` | stato `{objective,active,steps}` | JSON |
| `AI.GOALTICK()` | impulso di continuazione idle | passi (0 se inattivo) |
| `AI.GOALCLEAR()` | disattiva il goal | 1/0 |
| Funzione | Sintassi | Ritorna |
|---|---|---|
| `AI.REPLAYSTART(sessionId)` | avvia la registrazione | 1/0 |
| `AI.REPLAYRECORD(event)` | appende un evento | 1/0 |
| `AI.REPLAYSTOP()` | ferma la registrazione | 1/0 |
| `AI.REPLAYGET(sessionId)` | sequenza registrata | JSON |
| `AI.REPLAYLIST()` | sessioni registrate | JSON |
| `AI.REPLAYRUN(sessionId, targetId)` | riproduce gli eventi in una nuova sessione | n. eventi |
| `AI.REPLAYCLEAR()` | svuota | numero sessioni |
| Funzione | Sintassi | Ritorna |
|---|---|---|
| `AI.METRICRECORD(agent, key, value)` | accumula una metrica | 1/0 |
| `AI.METRICGET(agent, key)` | somma per agente+chiave | numero |
| `AI.METRICLIST()` | metriche per agente | JSON |
| `AI.FLEETSTATS()` | metriche + totali di flotta | JSON |
| `AI.METRICRESET()` | svuota | numero agenti |
| Funzione | Sintassi | Ritorna |
|---|---|---|
| `AI.HASHLINE(content)` | righe taggate `N#HASH\| riga` | stringa |
| `AI.HASHEDIT(content, lineRef, newText)` | sostituisce la riga (verifica hash) | stringa (o `Error: ...`) |
| `AI.HASHVERIFY(content, lineRef)` | il tag è valido? | 1/0 |
| Funzione | Sintassi | Ritorna |
|---|---|---|
| `AI.CHECKPOINT(sessionId, state)` | salva/aggiorna lo stato | 1/0 |
| `AI.RESUME(sessionId)` | stato salvato | stringa |
| `AI.CHECKPOINTLIST()` | checkpoint salvati | JSON |
| `AI.CHECKPOINTCLEAR()` | svuota | numero rimossi |
| Funzione | Sintassi | Ritorna |
|---|---|---|
| `AI.ORCHSAVE([path])` | salva lo stato (default `config/orchestration.db`) | n. scope salvati |
| `AI.ORCHLOAD([path])` | riprende lo stato (registri sostituiti) | n. scope ripristinati |

IMPORT AI
SUB Main()
  AI.RESOURCESET("max_agents", "4")
  AI.TASKDEFINE("a", "analisi") : AI.TASKDEFINE("b", "sintesi")
  AI.TASKDEPENDS("b", "a")
  AI.TASKASSIGN("a", "researcher") : AI.TASKASSIGN("b", "writer")
  AI.PLANDEFINE("p", "report") : AI.PLANADD("p", "a") : AI.PLANADD("p", "b")
  AI.PLANAPPROVE("p")
  PRINT AI.PLANRUN("p")
END SUB


### 2.12 `AI VAULT` / `AI NOTE` — Second Brain L2 (vault + note) (2026-09-10)
| Comando | Sintassi | Effetto |
|---|---|---|
| `AI VAULT CREATE` | `AI VAULT CREATE <name>, <path>` | registra/aggiorna un vault sulla cartella `<path>` e lo rende attivo |
| `AI VAULT INDEX` | `AI VAULT INDEX <name>` | indicizza il vault (scansione file) |
| `AI VAULT WATCH` | `AI VAULT WATCH <name>, <AUTO\|MANUAL>` | abilita/disabilita il reindex on-change |
| `AI NOTE CREATE` | `AI NOTE CREATE <path> [, <content>] [, TAGS "a,b"]` | crea una nota (frontmatter `title`+`tags`) |
| `AI NOTE UPDATE` | `AI NOTE UPDATE <path>, <content>` | sostituisce il contenuto |
| `AI NOTE APPEND` | `AI NOTE APPEND <path>, <testo>` | accoda testo |
| `AI NOTE DELETE` | `AI NOTE DELETE <path>` | elimina la nota |
| `AI NOTE MOVE` | `AI NOTE MOVE <old>, <new>` | sposta/rinomina |
| `AI NOTE LINK` | `AI NOTE LINK <from> TO <to> [AS <alias>]` | link manuale persistito |
| `AI NOTE UNLINK` | `AI NOTE UNLINK <from> TO <to>` | rimuove il link |
| `AI NOTE SET ALIAS` | `AI NOTE SET ALIAS <from> TO <to> AS <alias>` | alias su un link |
| Funzione | Ritorna |
|---|---|
| `AI.NOTEGET(vault, path)` | corpo della nota |
| `AI.NOTETITLE(vault, path)` | titolo (frontmatter o nome file) |
| `AI.NOTETAGS(vault, path)` | tag (frontmatter) |
| `AI.NOTEWORDCOUNT(vault, path)` | numero parole |
| `AI.NOTEEXISTS(vault, path)` | 1/0 |
| `AI.NOTEFOLDER(vault, path)` | cartella della nota nel vault |
| `AI.NOTELINK(vault, from, to, alias)` / `AI.NOTEUNLINK(...)` / `AI.NOTESETALIAS(...)` | link manuali |

IMPORT AI

SUB Main()
  DIM v AS STRING = "work/note-vault"
  IF IO.DirExists(v) = 0 THEN MKDIR v
  AI VAULT CREATE "nv", v
  AI VAULT INDEX "nv"
  AI NOTE CREATE "intro.md", "hello world", TAGS "alpha,beta"
  AI NOTE CREATE "altra.md", "seconda nota"
  PRINT AI.NOTEGET("nv", "intro.md")      ' hello world
  PRINT AI.NOTETAGS("nv", "intro.md")     ' [alpha beta]
  AI NOTE LINK "intro.md" TO "altra.md" AS "vedi"
END SUB


### 2.13 Funzioni AI avanzate (blackboard, health-check, immagini, pesi, vault)

AI.BLACKBOARDWRITE(board, key$, value)   ' scrive una chiave
PRINT AI.BLACKBOARDREAD(board, key$)     ' legge
PRINT AI.BLACKBOARDWAIT(board, key$)     ' attende che la chiave compaia (park-based)


AI.STARTHEALTHCHECK(name, seconds)       ' avvia il monitoraggio periodico
AI.STOPHEALTHCHECK(name)                 ' ferma
PRINT AI.HEALTHCHECKINTERVAL(name)       ' intervallo configurato
AI.RUNHEALTHCHECK(name)                  ' esegue subito un controllo
AI.RESETFAILED(name)                     ' ripristina i provider marcati falliti


AI.SETWEIGHT(name, weight)               ' imposta il peso
PRINT AI.WEIGHT(name)                    ' legge il peso


PRINT AI.IMAGEINFO(path)                 ' dimensioni/formato
AI.IMAGERESIZE(path, w, h, out$)         ' ridimensiona
AI.IMAGECROP(path, x, y, w, h, out$)     ' ritaglia
AI.IMAGECONVERT(path, format$, out$)     ' converte formato


AI.GOALAGENT(agent, goal$)               ' assegna un obiettivo all'agente
AI.PLANCLEAR(agent)                      ' azzera il piano
AI.TASKCLEAR(agent)                      ' azzera i task


AI.VAULTREMOVEFOLDER(vault, folder$)     ' rimuove una cartella dal vault
AI.VAULTSETFOLDERPROPS(vault, folder$, props)
PRINT AI.INDEXMETA(vault)                ' metadati dell'indice


### 2.13 Second Brain (VAULT/NOTE/LINK/BRAIN) e Vector — funzioni AI.*
| Funzione | Ritorna |
|---|---|
| `AI.VAULTINFO(name)` / `AI.VAULTFOLDERS(name)` / `AI.VAULTCOUNT(name)` / `AI.VAULTLIST()` | info/lista vault (JSON, CSV) |
| `AI.NOTEGET/NOTETITLE/NOTETAGS/NOTEWORDCOUNT/NOTEEXISTS/NOTEFOLDER(vault, path)` | metadati/lettura nota |
| `AI.NOTELINK/NOTEUNLINK/NOTESETALIAS(vault, from, to, …)` | link manuali |
| `AI.LINKSOUT/BACKLINKS(vault, path)`, `AI.LINKJSON(vault)`, `AI.LINKDANGLING(vault)` | grafo dei link |
| `AI.BRAINLIST()` / `AI.BRAINUSED()` / `AI.BRAININFO(name)` | brain registrati/usati/info |
| `AI.ENSURECOLLECTION(db, coll, dims)` / `AI.INDEXTEXT(db, coll, src, text)` / `AI.INDEXMETA(…)` | vector DB (indice) |
| `AI.COUNT(db, coll)` / `AI.LISTCOLLECTIONS(db)` / `AI.SEARCH(db, coll, query, topK)` / `AI.RERANK(query, hits, topK)` | query |
| `AI.REMOVESOURCE(db, coll, src)` / `AI.PURGEDEPRECATED(db, coll)` / `AI.LOADDOCUMENTS(db, coll, path)` | manutenzione |
| `AI.EMBED(text, dims)` | embedding (vettore) |

## 3. Esempio end-to-end — provider locale + agente + RAG + OCR

IMPORT AI

SUB Main()
  ' 1) Registra provider locali (AIPATH) e remoto
  AI REGISTER "coder", %CHAT, "mlx", "{""modelDir"":""/Users/gaetano/ai/Models/Qwen3-Coder-30B-A3B-Instruct-8bit""}"
  AI REGISTER "ocr", %OCR, "PaddleOCR-VL-1.6-GGUF"
  AI SET ACTIVE %CHAT, "coder"
  AI SET ACTIVE %OCR, "ocr"

  ' 2) RAG: indicizza e chiedi
  AI RAG ENSURE COLLECTION "kb", "docs", 768
  AI RAG INDEX TEXT "kb", "docs", "gpm", "La GPM è l'unico esecutore del codice BASIC."
  PRINT AI.ASK("kb", "docs", "Chi esegue il codice BASIC?", 2)

  ' 3) Agente con tool RAG + OCR
  AI AGENT CREATE Analista


## 4. Stato implementazione (per manutentori, non per l'uso quotidiano)

## 5. Visioni unificate (per chi vuole spingersi oltre)

## 6. Riferimenti

## Copertura per namespace — CATEGORY / HOOK / ROTATE / ASKCHAIN / WORKFLOW / TEAM / VECTOR (2026-09-15)
| Funzione | Cosa fa | Verifica |
|---|---|---|
| `AI.CATEGORYSET(categoria, provider)` | assegna il provider di una categoria | `gen_AI_CategorySet.bas` |
| `AI.CATEGORYLIST()` | elenco categorie→provider | `gen_AI_CategoryList.bas` |
| `AI.CATEGORYPICK(categoria)` | provider scelto per la categoria | `gen_AI_CategoryPick.bas` |
| `AI.CATEGORYAUTO(n)` / `AI.CATEGORYFALLBACK(cat, prov)` | categorizzazione automatica / fallback | `gen_AI_CategoryAuto.bas`, `gen_AI_CategoryFallback.bas` |
| `AI.HOOKREGISTER(evento, handler)` / `AI.HOOKUNREGISTER(...)` | registra/rimuove un handler di evento | `gen_AI_HookRegister/Unregister.bas` |
| `AI.HOOKLIST()` / `AI.HOOKEVENTS()` | elenco hook registrati / eventi disponibili | `gen_AI_HookList/Events.bas` |
| `AI.HOOKFIRE(evento[, payload])` | emette un evento (il payload è opzionale) | `gen_AI_HookFire.bas` |
| `AI.ROTATE(role)` | ruota il provider del ruolo (provider/registry) | `gen_AI_Rotate.bas` |
| `AI.ASKCHAIN(prompt, agente[, modalità])` | prompt a catena con stato su blackboard; **agente inesistente → errore esplicito** `ERROR: Agent '<nome>' is not registered.` (parità IL) | `gen_AI_AskChain.bas` |
| `AI.WORKFLOWLIST()` | elenco workflow definiti | `gen_AI_WorkflowList.bas` |
| `AI.TEAMLIST()` | elenco dei team | `gen_AI_TeamList.bas` |
| `AI.SEARCH(db, collezione, query, topK, …)` | ricerca vettoriale (richiede ≥5 argomenti) | `gen_AI_Search.bas` |
| `AI.COUNT(db, collezione)` | numero di elementi indicizzati | `gen_AI_Count.bas` |

### Famiglia LLM-dipendente — comportamento senza provider (blocco 1, 2026-09-15)
| Funzione | Senza provider | Note |
|---|---|---|
| `AI.ENRICH(testo, modalità)` | `Error: nessun provider LLM configurato (AI SET ACTIVE)` | `Enricher.NoCompleter`; testo vuoto → `""` |
| `AI.SUGGESTTAGS(testo)` | `Error: nessun provider LLM configurato (AI SET ACTIVE)` | delega a `Enrich(..., Tagging)` |
| `AI.BRAINVAULTWRITE(brain, …)` | `Error: nessun provider LLM configurato (AI SET ACTIVE)` | `BrainVault.cs:133` |
| `AI.QAANSWER(domanda, brain[, window])` | `{"status":"Error","error":"brain '<b>' non disponibile come rag"}` se il brain non è un RAG; `{"status":"Error","error":"<NoCompleter>"}` se manca il provider | `Enricher.QaAnswer` |
| `AI.ANALYZEIMAGE(path, …)` | `ERROR: file immagine non trovato: <path>` (il FILE è validato **prima** del modello) | parità ordine dei controlli |

### Memoria e orchestrazione (blocco B, 2026-09-15)
| Funzione | Cosa fa | Riferimento |
|---|---|---|
| `AI.MEMORYRANK(query[, topK])` | ricerca nel brain/memory: ritorna i passaggi più rilevanti (topK default 5) | `gen_AI_MemoryRank.bas` |
| `AI.MEMORYCONSOLIDATE()` | consolida la memoria del brain; ritorna 1 in caso di successo | `gen_AI_MemoryConsolidate.bas` |
| `AI.ORCHLOAD([path])` | carica lo stato dell'orchestrazione; ritorna il **numero di scope** caricati (0 = nessuno) | `gen_AI_OrchLoad.bas` |
| `AI.ANALYZEIMAGE(path, prompt)` | analisi immagine; **il path è validato PRIMA del modello** → `ERROR: file immagine non trovato: <path>` | `gen_AI_AnalyzeImage.bas` |
| `AI.NOTEUNLINK(vault, da, a)` / `AI.NOTESETALIAS(vault, da, a, alias)` | rimuove un link / imposta l'alias di un link (ritorno **numerico**) | `gen_AI_NoteUnlink.bas`, `gen_AI_NoteSetAlias.bas` |
| `AI.TASKDEFINE/…` (7) e `AI.PLANDEFINE/…` (7) | Task Graph con dipendenze e Plan: erano già implementati, resi raggiungibili dal fix del dispatch | `gen_AI_Task*.bas`, `gen_AI_Plan*.bas` |

### Gruppo B — BrainVaultBuild e Team run (2026-09-16)
| Funzione | Comportamento | Note |
|---|---|---|
| `AI.BRAINVAULTBUILD(dir)` | dir vuota → `-1`; senza provider embedding → errore esplicito ("BrainVaultBuild richiede un provider embedding attivo (AI SET ACTIVE %EMBEDDING, alias)"); altrimenti n° pagine indicizzate (`0` con repo AI vuoto) | implementazione condivisa nei 4 backend; in C genera le pagine solo il linker C# |
| `AI.TEAMRUNSTART(team, task)` | avvia il run del team e ritorna l'handle `team_<n>`; team mancante → `ERROR: Team 'x' non trovato (usa TEAM CREATE prima).` | run sincrono sul chiamante (GPM §0ter); l'handle simula l'async |
| `AI.TEAMRUNWAIT(handle)` | ritorna il risultato del run e **consuma** l'handle; handle sconosciuto → `Error: handle team sconosciuto 'x' (TeamRunStart non chiamato o già consumato)` | un solo Wait per handle |
| `AI.TEAMONSTREAM(sub)` | registra la callback di stream; idempotente, ritorna sempre `1`, mai errori | solo registrazione (GPM dispatch) |

### Provenance — firma e condivisione (2026-09-15)
| Funzione | Cosa fa |
|---|---|
| `AI.SIGNNOTE(note[, chiave])` | firma una nota (SHA-256) e la registra nella provenance |
| `AI.VERIFYNOTE(note[, chiave])` | verifica la firma → `1` / `0` |
| `AI.SHAREENCRYPT(content, chiave)` | ritorna **base64(AES-256-GCM(salt|nonce|tag|ciphertext))** |
| `AI.SHAREDECRYPT(cipherBase64, chiave)` | decifra; base64 non valido **o** chiave errata → `ERROR: The input is not a valid Base-64 string …` (messaggio dell'oracolo, mai stringa vuota silenziosa) |

### Ciclo di vita degli agenti e code (2026-09-15)
| Comando/funzione | Cosa fa |
|---|---|
| `AI.AGENTLIST()` | elenco degli agenti come array (`[]` se nessuno) |
| `AI.AGENTPAUSE(nome)` / `AI.AGENTRESUME(nome)` | sospende / riprende un agente |
| `AI.AGENTSTOP(nome)` | ferma un agente |
| `AI.AGENTRUN(nome, obiettivo[, passi])` | esegue il loop Brain→Azione dell'agente (agente **inesistente** → `ERROR: Agent '<nome>' is not registered.`) |
| `AI.AGENTSEND(nome, da, task, input)` | invia un task alla coda dell'agente |
| `AI.AGENTRECEIVE(nome)` | preleva il prossimo risultato dalla coda |
| `AI.AGENTQUEUESTATS(nome)` | stato della coda: `{"agent":…,"count":…,"paused":…,"stopped":…}` |
| `AI.AGENTQUEUECLEAR(nome)` | svuota la coda |

### Brain, skills, vault (2026-09-15)
| Comando/funzione | Cosa fa |
|---|---|
| `AI.BRAINUSE(nomi)` / `AI.BRAINDETACH(nome)` | collega / scollega uno o più brain (nomi separati da virgola) |
| `AI AGENT SET <agente>, brains, "a AS x, b"` | **livello AGENT dello scope**: l'agente vede SOLO i cervelli dichiarati (grant); `AI.AGENTGET(<agente>, "brains")` li rilegge. Senza dichiarazione eredita i cervelli collegati (`AI BRAIN USE`). Un tool che chiede un cervello fuori dal grant → **errore esplicito** (nessuna query silenziosa) |
| `AI.VAULTWATCH(nome[, policy])` | imposta la policy di indicizzazione del vault (`AUTO`/`MANUAL`) e ritorna **1** (anche se il vault non esiste ancora, come l'oracolo) |
| `AI.SKILLSPROMPT()` | documento markdown dell'archivio skills (percorsi, elenco) |
| `AI.HFREPO()` | repository HuggingFace configurato |

### Worker isolato e Context Engine (2026-09-16)
| Comando / funzione | Cosa fa |
|---|---|
| `AI WORKER NEW "nome", {"workspace":"work/x","budget":16000}` | crea il worker, il suo workspace su disco e lo rende **corrente** |
| `AI WORKER "ANALYZE"` | **invocazione pulita** del worker corrente: la vista inviata è `TASK` + `plan.md` + `notes.md` + `solution` + `state/*` del workspace — mai la chat history |
| `AI WORKER RUN "nome"` | round di lifecycle sul worker indicato (aggiorna solo i contatori) |
| `AI WORKER LOOP "ANALYZE"[, rounds]` | **loop isolato multi-round** (default 3): ogni round vede la memoria aggiornata dal precedente; il budget del worker è il tetto **cumulativo dell'intero run**; il risultato migliore resta in `AI.STATELOAD("nome","best")` |
| `AI REPLAY AUTO "sessione"` | riesegue **automaticamente** gli eventi registrati in una sessione di replay, valutandoli col runtime: gli esiti per-evento ("ok: …" / "error: …") vanno in `AI.REPLAYGET("sessione#auto")` — un evento non eseguibile è tracciato, mai ignorato |
| `AI WORKER MANAGE ["piano"] [, cicli] [REQUIRE "…"]` | **manager loop sul task graph**: ad ogni ciclo prende il prossimo task lavorabile (dipendenze soddisfatte), lo esegue col worker e lo marca `done` se l'auditor `REQUIRE` lo accetta; se lo rifiuta **logga il dissenso** (`AI.STATELOAD("nome","dissents")`) e ritenta. Con il **piano** limita il lavoro ai task del piano (che deve essere **approvato**: altrimenti `-5`). Esiti in `AI.STATELOAD("nome","manage_exit")`: `1` fatto · `0` budget del run · `-1` nessun worker · `-2` errore provider · `-3` cicli esauriti · `-4` deadlock · `-5` piano non approvato |
| `AI WORKER LOOP "VERIFY", 3 REQUIRE "LEN(RESULT$) > 5"` | **loop verificato**: dopo ogni round un **auditor indipendente** valuta la clausola `REQUIRE` con `RESULT$` = risposta. Vera → stop verificato (`state/verified = 1`); falsa → **rework informato** (l'esito entra in `state/audit_error` e il round successivo lo vede); mai vera → esito `-3` |
| `AI STATE SAVE "nome", "notes", "testo"` | scrive una voce della memoria esterna del run (il worker la ritrova nell'invocazione successiva) |
| `AI.STATELOAD("nome", "notes")` | legge una voce (contiene anche `solution` e `last_error` dell'ultima invocazione) |
| `AI CONTEXT SET BUDGET "worker:nome", 16000` | budget in caratteri della vista di quel worker |
| `AI CONTEXT SET POLICY "worker:nome", "error"` | `compact` (default) / `truncate` / `error`: cosa fare se la vista supera il budget |
| `AI CONTEXT VIEW "worker:nome", "testo"` | imposta a mano la vista curata (normalizzata subito secondo budget+policy) |
| `AI.CONTEXTUSED/…BUDGET/…OVERFLOW("worker:nome")` | caratteri della vista reale, budget configurato, caratteri del testo originale oltre budget |
| `AI.CONTEXTVIEW$("worker:nome")` | la vista **realmente** inviata (ispezione) |
| `AI CONTEXT CREATE "agent"` | **§3.6 PARSEC, lato AGENTE** — azzera lo **store di segmenti** del context builder |
| `AI CONTEXT ADD "agent", "<kind>", "testo"` | appende un segmento. `<kind>` = `SYSTEM` \| `HISTORY` \| `TOOL` \| `OTHER` (normalizzato, case-insensitive); `SYSTEM` è *pinned* (mai evinto) |
| `AI CONTEXT MAP "agent"` | stampa la **mappa**: `segs`, `chars`, `budget`, `overflow` + chars per kind (report) |
| `AI CONTEXT TRIM "agent"` | elimina i segmenti **non-SYSTEM più VECCHI** finché il totale ≤ budget |
| `AI CONTEXT GC "agent"` | **eviction**: rimuove i segmenti **duplicati** (stesso kind+testo) + l'`HISTORY` oltre gli ultimi 8 |
| `AI CONTEXT COMMIT "agent"` | costruisce la **vista curata** = segmenti (SYSTEM per primi, poi gli altri in ordine), normalizzata secondo budget+policy → leggibile con `AI.CONTEXTVIEW$` |
| `AI.CONTEXTHASH$("agent")` | **§3.6 PARSEC** — hash FNV-1a (hex 16) della vista curata: **chiave della prefix-cache KV** (se la vista cambia, l'hash cambia → cache invalidata) |
| `AI.CONTEXTVALID("agent", "rev")` | `1` se `"rev"` coincide col hash corrente della vista (accetta il hash nudo o `"rev=<hash>"`), altrimenti `0` — **invalidazione per version hash** |
| `AI.TASKMETRICS$("task")` | **§3.6 PARSEC** — metriche TASK standard come JSON: `{"tokens":..,"ttft":..,"tool_calls":..,"llm_calls":..,"latency":..}` (registrate via `AI.METRICRECORD`) |
| `AI.WORKERROUNDS()` / `AI.WORKERRESCUED()` / `AI.WORKERSPEND()` | invocazioni tentate / fallite-annotate / caratteri di vista consumati |
| esito | significato |
|---|---|
| `1` | invocazione eseguita: l'esito è in `<workspace>/solution` e in `AI.STATELOAD("nome","solution")` |
| `0` | **rifiutata dal budget** (policy `error`): `AI.STATELOAD("nome","last_error")` = `context overflow: N > budget M`, contata come rescue |
| `-1` | nessun worker corrente (manca `AI WORKER NEW`) |
| `-2` | errore del provider: `last_error` = `Error: nessun provider LLM configurato (AI SET ACTIVE)` |
| `0` (loop) | budget del **run** esaurito: `last_error` = `run budget exhausted: spent=S budget=B` (nessun round tentato a vuoto) |
| `-3` (loop verificato) | l'audit `REQUIRE` non è mai stato soddisfatto: `last_error` = `audit failed after N round(s)` |

## Benchmark e telemetria della sessione KV (2026-09-18)

### `AI BENCHMARK <alias>, PREFILL|DECODE|ALL [, {json}]` — comando (azione)

### `AI.BENCHRESULT(<alias>)` — funzione (lettura)

AI AGENT CREATE "bench-agent"
AI BENCHMARK "bench-agent", ALL, {iterations:3, text:"Benchmark di prefill e decode."}
DIM b$ = AI.BENCHRESULT("bench-agent")


### `AI.PREFILLSTATS(<alias>)` / `AI.DECODESTATS(<alias>)` — funzioni (lettura)

## Canali pub/sub degli agenti

### `AI.CHANNELCREATE(<canale>)` — funzione

### `AI.CHANNELPUBLISH(<canale>, <messaggio>)` — funzione

### `AI.CHANNELSUBSCRIBE(<canale>, <handler>)` / `AI.CHANNELUNSUBSCRIBE(<canale>, <handler>)` — funzioni

DIM ch = AI.CHANNELCREATE("demoChan")
DIM s  = AI.CHANNELSUBSCRIBE("demoChan", "demoAgent")
DIM p  = AI.CHANNELPUBLISH("demoChan", "hello from channel")
DIM u  = AI.CHANNELUNSUBSCRIBE("demoChan", "demoAgent")


## LLM.SYMBOL / LLM.IMPACT / LLM.CALLMAP — grafo semantico del PROGRAMMA (§4.10f)
| Funzione | Parametri | Ritorna |
|---|---|---|
| `LLM.SYMBOL("nome")` | nome simbolo | `"<kind> <label>"` (es. `"procedure Worker"`) |
| `LLM.CALLMAP("nome")` | nome simbolo | `"callers=[...] callees=[...]"` (chi chiama / chi è chiamato) |
| `LLM.IMPACT("nome"[, <profondità>])` | nome + profondità (default 32) | `"chiamante@1,chiamante@2,..."` (impatto transitivo, a livelli) |

SUB Worker()
  Helper()          ' Worker chiama Helper
END SUB
SUB Helper()
END SUB
SUB Main()
  PRINT LLM.SYMBOL("Worker")    ' procedure Worker
  PRINT LLM.CALLMAP("Worker")   ' callers=[Main] callees=[Helper]
  PRINT LLM.IMPACT("Helper")    ' Worker@1,Main@2
END SUB


### AI-Documenti-Guide

*Fonte: `AI-Documenti-Guide`*


## 1. Campi fattura offline — AI.DOCFIELDS(path) → JSON

PRINT AI.DOCFIELDS("fattura.txt")
' {"number":"2026/001","date":"11/09/2026","total":1234.56,"vat":"01234567890"}


## 2. Estrazione con schema via LLM — AI.DOCEXTRACT(path, schema) → JSON

DIM schema AS STRING
schema = "{""total"":""number"",""fornitore"":""string""}"
PRINT AI.DOCEXTRACT("fattura.pdf", schema)


## 3. Errori

## 4. Implementazione (riferimento)

### AI-Immagini-Guide

*Fonte: `AI-Immagini-Guide`*


## 1. Manipolazione (offline, nessun modello)
| Funzione | Firma | Ritorno |
|---|---|---|
| `AI.IMAGEINFO` | `(path)` | STRING `"larghezza,altezza,formato"` (es. `"4,4,png"`) |
| `AI.IMAGERESIZE` | `(input, output, width, height)` | LONG `1` ok / `0` errore |
| `AI.IMAGECROP` | `(input, output, x, y, width, height)` | LONG `1` ok / `0` errore |
| `AI.IMAGECONVERT` | `(input, output)` | LONG `1` ok / `0` errore (normalizza a PNG) |

IMPORT AI

SUB Main()
    PRINT AI.IMAGEINFO("foto.png")                    ' "640,480,png"
    ok = AI.IMAGERESIZE("foto.png", "work/mini.png", 320, 240)
    ok = AI.IMAGECROP("foto.png", "work/ritaglio.png", 10, 10, 100, 100)
    ok = AI.IMAGECONVERT("foto.png", "work/copia.png")
END SUB


## 2. Visione via provider (richiede modello)
| Funzione | Firma | Note |
|---|---|---|
| `AI.OCR` | `(path [, model] [, redact])` | testo dall'immagine; provider `%OCR` (`AI SET ACTIVE %OCR, alias`) |
| `AI.ANALYZEIMAGE` | `(path, prompt)` | descrizione libera; riusa l'OCR se registrato |
| `AI.TRANSCRIBE` | `(path)` | audio→testo; provider `%AUDIO` (stessa famiglia) |

PRINT AI.OCR("fattura.png")
PRINT AI.ANALYZEIMAGE("foto.jpg", "Descrivi gli oggetti")


## 2bis. Embedding, compare, detect, classify (F5)
| Funzione | Firma | Ritorno | Modello |
|---|---|---|---|
| `AI.IMAGEEMBED` | `(path)` | ARRAY (64 double unitari) | **no** (percettivo) |
| `AI.IMAGECOMPARE` | `(path1, path2)` | LONG `0..100` / `-1` errore | **no** (percettivo) |
| `AI.IMAGEDETECT` | `(path)` | STRING (JSON bbox) | sì (`%OCR`) |
| `AI.IMAGECLASSIFY` | `(path, labels)` | STRING (etichetta) | sì (`%OCR`) |

DIM e() = AI.IMAGEEMBED("foto.png")        ' 64 elementi
PRINT AI.IMAGECOMPARE("a.png", "b.png")     ' 0..100 (a==a → 100)
PRINT AI.IMAGEDETECT("foto.jpg")            ' JSON [{label,confidence,x,y,width,height}]
PRINT AI.IMAGECLASSIFY("doc.png", "fattura,ricevuta,documento")


## 2ter. Video — `AI.VIDEO*` (F7)
| Funzione | Firma | Ritorno |
|---|---|---|
| `AI.VIDEOINFO` | `(path)` | STRING `"width,height,fps,frames"` |
| `AI.VIDEOFRAME` | `(path, index, output)` | LONG `1`/`0` (frame `index` → PNG) |
| `AI.VIDEOSNAPSHOT` | `(path, output)` | LONG `1`/`0` (primo frame → PNG) |

PRINT AI.VIDEOINFO("clip.y4m")                       ' "320,240,25,750"
ok = AI.VIDEOFRAME("clip.y4m", 10, "work/frame10.png")
ok = AI.VIDEOSNAPSHOT("clip.y4m", "work/cover.png")


## 3. Errori

## 4. Implementazione (riferimento)

### AI-Memory-Guide

*Fonte: `AI-Memory-Guide`*


## 1. Concetti
| Termine | Significato |
|---|---|
| `brain.db` | SQLite `<configDir>/brain.db` (default `<root>/config/brain.db`, configurabile) — persistenza unica |
| `kind` | `note` (default), `fact`, `episodic` |
| `watched` | cartella osservata (hash SHA256 per dedup) |

## 2. Comandi — `AI MEMORY`

### `AI MEMORY NOTE testo`

AI MEMORY NOTE "L'utente preferisce risposte concise."
AI MEMORY NOTE "La GPM è l'unico esecutore del codice BASIC."


### `AI MEMORY IMPORT path`

AI MEMORY IMPORT "/Users/gaetano/note.md"
AI MEMORY IMPORT "/Users/gaetano/Documents/AiWiki"


### `AI MEMORY WATCH path`

AI MEMORY WATCH "/Users/gaetano/Documents/AiWiki/01-Projects/GMBasic"


### `AI MEMORY REFRESH`

AI MEMORY REFRESH


### `AI MEMORY SAVE path` / `FORGET id`

AI MEMORY SAVE "memoria.json"
AI MEMORY FORGET "12"


## 3. Funzioni
| Funzione | Ritorna | Esempio |
|---|---|---|
| `AI.MEMORYASK(query [, topK])` | stringa JSON | `r$ = AI.MEMORYASK("linguaggio preferito?", 5)` |
| `MEMORY.SEARCH(query, topK)` | lista | `MEMORY.SEARCH("gpm", 5)` (legacy) |
| `MEMORY.LIST()` / `STORE` / `LOAD` / `DELETE` | — | alias legacy |

### Namespace `MEMORY` (KV legacy — tabella sotto)
| Funzione | Parametri | Ritorna | Note |
|---|---|---|---|
| `MEMORY.STORE(key$, value, isLongTerm)` | `key` chiave (string); `value` valore; `isLongTerm` `-1`/`0` (memoria a lungo termine) | **`-1` (ok) / `0`** (booleano canonico) | inserisce o sovrascrive |
| `MEMORY.LOAD(key$)` | `key` | stringa (valore, `""` se assente) | |
| `MEMORY.SEARCH(pattern$)` | `pattern` | stringa (nomi chiave separati da newline) | cerca sui **nomi delle chiavi** |
| `MEMORY.LIST()` | — | stringa (nomi chiave separati da newline) | |
| `MEMORY.DELETE(key$)` | `key` | **`-1` (ok) / `0`** | |

IMPORT MEMORY
MEMORY.STORE("utente", "Ada", 0)
PRINT MEMORY.LOAD("utente")           ' Ada
IF MEMORY.DELETE("utente") THEN PRINT "rimosso"   ' -1 = vero


IMPORT AI

SUB Main()
  AI MEMORY NOTE "La GPM comanda il gioco."
  AI MEMORY IMPORT "/Users/gaetano/Documents/AiWiki/01-Projects/GMBasicMonoRepo/Guide/utente/AI-Namespace-Manual.md"
  AI MEMORY WATCH "/Users/gaetano/Documents/AiWiki/01-Projects/GMBasicMonoRepo/Guide"

  PRINT AI.MEMORYASK("chi comanda il gioco?", 3)

  AI MEMORY REFRESH
  PRINT AI.MEMORYASK("esecutore codice BASIC", 3)
  PRINT AI.MEMORYASK("pipeline documento", 3)
END SUB
Main()


## 4. Riferimenti

### AI-OCR-Audio-Guide

*Fonte: `AI-OCR-Audio-Guide`*


## 1. Prerequisiti — provider attivo per capability
| Capability | Costante | Funzione che lo usa |
|---|---|---|
| OCR / visione | `%OCR` (1) | `AI.OCR`, `AI.ANALYZEIMAGE`, `CONVERT.EXTRACT` su pagine scansionate |
| Audio | `%AUDIO` (2) | `AI.TRANSCRIBE` |

' OCR locale PaddleOCR-VL (GGUF + mmproj via AIPATH)
AI REGISTER "ocr-local", %OCR, "PaddleOCR-VL-1.6-GGUF"
AI SET ACTIVE %OCR, "ocr-local"

' Audio locale Qwen2.5-Omni
AI REGISTER "audio-local", %AUDIO, "qwen2.5-omni-3b-Q4_K_M"
AI SET ACTIVE %AUDIO, "audio-local"

' Remoto (visione via OpenAI)
AI REGISTER "gpt4o", %OCR, "openai:gpt-4o"
AI SET ACTIVE %OCR, "gpt4o"


## 2. Funzioni

### `AI.OCR(path [, model [, redact]]) → stringa`

' Base
t$ = AI.OCR("scansione.png")

' Con modello esplicito
t$ = AI.OCR("scansione.png", "ocr-local", 0)

' Con anonimizzazione PII (email, telefono, CF, P.IVA, carta, IBAN, date, IP, indirizzi → [EMAIL]... )
t$ = AI.OCR("contratto.png", "Extract all text.", 1)
' → "Contatto [EMAIL] tel [PHONE] CF [TAX_ID] ..."


### `AI.TRANSCRIBE(path) → stringa`

PRINT AI.TRANSCRIBE("chiamata.wav")
PRINT AI.TRANSCRIBE("nota.mp3")


### `AI.ANALYZEIMAGE(path, prompt) → stringa`

PRINT AI.ANALYZEIMAGE("screenshot.png", "Descrivi il contenuto.")
PRINT AI.ANALYZEIMAGE("grafico.png", "NEXT STEP")


## 2bis. Sintesi vocale — `AI.SYNTHESIZE` (TTS)

ok = AI.SYNTHESIZE("Buongiorno, come posso aiutarla?", "work/ai_tts.wav")
PRINT ok          ' 1 = WAV scritto, 0 = errore


## 3. Integrazione

## 4. Riferimenti

## Navigazione canonica — AI embedded

### AI-Providers-Guide

*Fonte: `AI-Providers-Guide`*


## 1. Panoramica

## 2. `PROVIDER.REGISTER` — sintassi

REGISTER(alias, capability, source [, param])

| argomento | obbligatorio | significato |
|---|---|---|
| `alias` | sì | nome univoco del provider |
| `capability` | sì (remoti), opz. (embedded) | `%CHAT`/`%OCR`/`%AUDIO` |
| `source` | sì | `hf:<repo>`, path locale, `http(s)://`, scheme registry |
| `param` | dipende | cartella dest (per `hf:`), modello (per remoto), oppure JSON di config del provider |

### 2.1 Modelli embedded da scaricare (`hf:`)

' CHAT — MLX community
PROVIDER.REGISTER("ds32", %CHAT, "hf:mlx-community/DeepSeek-R1-Distill-Qwen-32B-MLX-8Bit")
PROVIDER.REGISTER("ds32", %CHAT, "hf:mlx-community/DeepSeek-R1-Distill-Qwen-32B-MLX-8Bit", "~/ai/Models")

' CHAT — GGUF (richiede --file nel downloader se multi-quant)
PROVIDER.REGISTER("qw", %CHAT, "hf:bartowski/Qwen2.5-Coder-32B-Instruct-GGUF", "~/ai/Models/gguf")

' OCR — PaddleOCR-VL
PROVIDER.REGISTER("ocr", %OCR, "hf:PaddlePaddle/PaddleOCR-VL-1.6-GGUF")
PROVIDER.REGISTER("ocr", %OCR, "hf:PaddlePaddle/PaddleOCR-VL-1.6-GGUF", "~/ai/Models/gguf")

' AUDIO — Qwen2.5-Omni (mmproj audio)
PROVIDER.REGISTER("audio", %AUDIO, "hf:ggml-org/Qwen2.5-Omni-3B-GGUF", "~/ai/Models/gguf")


### 2.2 Modelli embedded già su disco (path locale)

AI REGISTER "ocr", %OCR, "~/ai/Models/gguf/PaddleOCR-VL-1.6-GGUF.gguf"
AI REGISTER "ds32", %CHAT, "~/ai/Models/DeepSeek-R1-Distill-Qwen-32B-MLX-8Bit"
AI REGISTER "audio", %AUDIO, "~/ai/Models/gguf/qwen2.5-omni-3b-Q4_K_M.gguf"
AI SET ACTIVE PROVIDER %CHAT, "ds32"


### 2.2bis Motore MLX embedded — DeepSeek V4 Flash (DS4) e routing `.gguf`

' Auto-route: il .gguf di DS4 è riconosciuto (arch deepseek4) → motore MLX embedded.
AI REGISTER "ds4", "completion", "/Users/gaetano/ds4/ds4flash.gguf", _
    "{""maxTokens"":""32"",""temperature"":""0"",""contextSize"":""4096""}"
AI SET ACTIVE "completion", "ds4"   ' oppure: AI SET ACTIVE %CHAT, "ds4"
PRINT AI.ACTIVE(%CHAT)              ' → ds4

' Override esplicito del motore (forza il motore MLX):
AI REGISTER "ds4", "completion", "/Users/gaetano/ds4/ds4flash.gguf", "{""engine"":""mlx"",""maxTokens"":""16""}"

| chiave `param` (motore MLX/DS4) | default | significato |
|---|---|---|
| `engine` | auto | `"mlx"` forza il motore MLX embedded; `"llamacpp"` forza llama.cpp |
| `maxTokens` | 512 | tetto token per generazione (il path **agente** usa però `AI AGENT SET ... maxtokens`) |
| `temperature` | greedy | 0/assente = greedy/argmax |
| `contextSize` | 32768 | contesto (il motore MLX carica il modello interamente; il `.gguf` è mmap) |
| `swap` / `diskSwap` / `lazyWeights` | false | **swap su disco dei PESI MLX**: pesi lazy/file-backed (mmap), paginati dal SO on-demand — per modelli che non entrano interamente in RAM |
| `kvSwapEnabled` / `kvSwapMaxHotSessions` / `kvSwapMaxKvMb` / `kvSwapPath` | false | **swap su disco della KV/sessione** (MLX e llama.cpp) |

' I pesi restano LAZY/file-backed (mmap): il SO li pagina da disco on-demand.
AI REGISTER "big", "completion", "<dir-modello>", "{""engine"":""mlx"",""swap"":""true""}"


### 2.3 Modelli remoti via API (URL)

PROVIDER.REGISTER("gpt",    %CHAT, "https://api.openai.com/v1", "gpt-4o")
PROVIDER.REGISTER("gpt-ocr", %OCR, "https://api.openai.com/v1", "gpt-4o")     ' visione remota
PROVIDER.REGISTER("claude",  %CHAT, "https://api.anthropic.com/v1", "claude-sonnet")
PROVIDER.REGISTER("gemma",   %CHAT, "http://localhost:11434", "gemma4:31b-mlx") ' ollama


### 2.4 Modelli remoti via scheme registry (config)

PROVIDER.REGISTER("gemma", %CHAT, "ollama:gemma4:31b-mlx")
PROVIDER.REGISTER("gpt",   %CHAT, "openai:gpt-4o")
PROVIDER.REGISTER("claude", %CHAT, "anthropic:claude-sonnet")


### 2.5 Capability opzionale per embedded

PROVIDER.REGISTER("ocr", %OCR, "~/ai/Models/gguf/qualcosa.gguf")   ' forza OCR


## 3. `PROVIDER.ACTIVE` — attivazione per capability

' Setter: attiva un provider per capability (dedotta se omessa)
PROVIDER.ACTIVE(%CHAT, "mio-chat")
PROVIDER.ACTIVE(%OCR, "mio-ocr")
PROVIDER.ACTIVE(%AUDIO, "mio-audio")
PROVIDER.ACTIVE("mio-chat")               ' capability dedotta dal tipo registrato

' Getter: qual è il provider attivo per quella capability
DIM attivo$ AS STRING
attivo$ = PROVIDER.ACTIVE(%OCR)           ' alias OCR attivo, o "" se nessuno


## 4. Comandi di scoperta e gestione

PROVIDER.LIST()                  ' tutti i provider, tutte le capability
PROVIDER.LIST("ocr")             ' solo i provider OCR
PROVIDER.UNREGISTER("mio-ocr")
PROVIDER.CONFIGURE("mio-ocr", "gpuLayers", "99")


## 5. Uso in BASIC — esempi completi

### 5.1 Chat embedded

PROVIDER.REGISTER("chat", %CHAT, "hf:mlx-community/Qwen3-Coder-30B-A3B-Instruct-8bit")
PROVIDER.ACTIVE(%CHAT, "chat")
CREATE AGENT Assistent
DIM risposta$ AS STRING
risposta$ = AIAGENT.ASK("Assistent", "Spiega il pattern observer in 3 righe")
PRINT risposta$


### 5.2 OCR — visione

PROVIDER.REGISTER("ocr", %OCR, "hf:PaddlePaddle/PaddleOCR-VL-1.6-GGUF")
PROVIDER.ACTIVE(%OCR, "ocr")
DIM testo$ AS STRING
testo$ = AI.OCR("foto.png")                    ' prompt default: estrai tutto il testo
testo$ = AI.OCR("foto.png", "OCR:")            ' prompt esplicito (recognition PaddleOCR)
PRINT testo$


### 5.3 Audio — trascrizione

PROVIDER.REGISTER("audio", %AUDIO, "hf:ggml-org/Qwen2.5-Omni-3B-GGUF")
PROVIDER.ACTIVE(%AUDIO, "audio")
DIM tr$ AS STRING
tr$ = AI.TRANSCRIBE("registrazione.wav")       ' wav/mp3/flac
PRINT tr$


### 5.4 Coesistenza chat + ocr + audio nello stesso programma

' tre modelli in linea, ognuno attivo sulla propria capability
PROVIDER.REGISTER("chat",  %CHAT,  "hf:mlx-community/Qwen3-Coder-30B-A3B-Instruct-8bit")
PROVIDER.REGISTER("ocr",   %OCR,   "hf:PaddlePaddle/PaddleOCR-VL-1.6-GGUF")
PROVIDER.REGISTER("audio", %AUDIO, "hf:ggml-org/Qwen2.5-Omni-3B-GGUF")
PROVIDER.ACTIVE(%CHAT, "chat")
PROVIDER.ACTIVE(%OCR, "ocr")
PROVIDER.ACTIVE(%AUDIO, "audio")

CREATE AGENT A
DIM doc$ AS STRING, parlato$ AS STRING, risp$ AS STRING
doc$ = AI.OCR("fattura.png")                   ' usa il provider OCR
parlato$ = AI.TRANSCRIBE("nota.wav")           ' usa il provider audio
risp$ = AIAGENT.ASK("A", "Riepiloga il contenuto della fattura e la nota.")  ' usa il provider chat
PRINT risp$


### 5.5 Errore se la capability non è attivata

PROVIDER.REGISTER("ocr", %OCR, "hf:PaddlePaddle/PaddleOCR-VL-1.6-GGUF")
' NIENTE ACTIVE(%OCR, ...)
DIM t$ AS STRING
t$ = AI.OCR("foto.png")
' -> "[AI OCR: no active OCR provider — call ACTIVE(%OCR, \"alias\") first]"


### 5.6 Download con progress dal REPL

' REPL (non BASIC): comandi HF/MLX esistenti, con progresso curl
HF PaddlePaddle/PaddleOCR-VL-1.6-GGUF --dest ~/ai/Models/gguf
MLX mlx-community/Qwen3-Coder-30B-A3B-Instruct-8bit --dest ~/ai/Models


## 6. Config unificata (`config/providers.json`)

{
  "schemes": {
    "hf":         "https://huggingface.co",
    "hf2":        "https://nuovo-registry.co",
    "ollama":     "http://localhost:11434",
    "openai":     "https://api.openai.com/v1",
    "anthropic":  "https://api.anthropic.com/v1"
  },
  "providers": [
    { "alias": "ds32", "capability": "chat", "kind": "mlx",
      "source": "hf:mlx-community/DeepSeek-R1-Distill-Qwen-32B-MLX-8Bit", "destDir": "~/ai/Models" },
    { "alias": "ocr", "capability": "ocr", "kind": "llamacpp-vision",
      "source": "hf:PaddlePaddle/PaddleOCR-VL-1.6-GGUF", "destDir": "~/ai/Models/gguf" }
  ],
  "active": { "chat": "ds32", "ocr": "ocr", "audio": null }
}


## 7. Modelli consigliati (catalogo di default)
| Capability | Modello | Scheme | Note |
|---|---|---|---|
| `%CHAT` | Qwen3-Coder-30B-A3B-Instruct-8bit | `hf:mlx-community/...` | MoE, coding, 8-bit |
| `%OCR` | PaddleOCR-VL-1.6 | `hf:PaddlePaddle/PaddleOCR-VL-1.6-GGUF` | OCR/visione, GGUF+mmproj |
| `%AUDIO` | Qwen2.5-Omni-3B | `hf:ggml-org/Qwen2.5-Omni-3B-GGUF` | audio in/out, mmproj audio |
| `%OCR` | Apple Vision (macOS) | — | zero download, kind `apple-vision` |
| `%OCR` | gpt-4o / claude (remoto) | `openai:`/`anthropic:` | visione remota |

## 8. Test

# Suite AI (env per modelli reali)
dotnet test AI.Tests --filter "Category=Fast"   # gai/gmo
dotnet test GMBasicSharp/tests/AI.Tests --filter "Category=Fast"  # gm

# Gate BASIC
cd test_run && dotnet run                        # gai/gmo: N OK, 0 ERROR
dotnet run --project tools/runners/GMBASIC.Conformance   # gm: PASS/FAIL


## 9. Retro-compat e note

## 10. PROVIDER.DISCOVER — carica i modelli di un'API key (es. OpenRouter)

' Scopre e registra tutti i modelli dell'API key (formato OpenAI data[] o OpenRouter models[])
PROVIDER.DISCOVER("or", %CHAT, "openrouter:...", "sk-...")        ' tutti
PROVIDER.DISCOVER("or", %CHAT, "openrouter:...", "sk-...", TRUE)  ' solo free

' Poi si assegnano priorità e si ruota (già esistente)
PROVIDER.SETPRIORITY("or:gpt-4o-free", 10)
PROVIDER.SETPRIORITY("or:llama-3.3-70b", 5)
PROVIDER.ACTIVE(%CHAT, "or:gpt-4o-free")
PROVIDER.ROTATE(%CHAT)          ' passa al successivo per priorità/peso


### Credit-based routing — passa al successivo a crediti esauriti

' Imposta il budget di crediti gratuiti (-1 = illimitato, default)
PROVIDER.SETCREDITS("or:gpt-4o-free", 100)
DIM restanti AS INTEGER
restanti = PROVIDER.CREDITS("or:gpt-4o-free")   ' crediti rimasti

' A ogni completamento i crediti calano; a 0 il provider va Blocked e
' ROTATE/ACTIVE passano automaticamente al successivo (per priorità, free-first)


## Navigazione canonica — AI embedded

### AI-Skills-Plugins-MCP-Guide

*Fonte: `AI-Skills-Plugins-MCP-Guide`*


## 1. Skill

AI SKILLS ADD "matematica"
AI SKILLS REMOVE "matematica"
AI SKILLS SET INDEX "~/skills"
AI SKILLS REFRESH
AI SKILLS MARKETPLACE "https://..."

s$ = AI.SKILLSPROMPT()                 ' prompt aggregato
DIM sl AS ARRAY OF STRING = AI.SKILLSLIST()
DIM sp AS ARRAY OF STRING = AI.SKILLSPATHS()

AI PATH SKILL ADD "/Users/gaetano/skills"
AI PATH SKILL REMOVE "/Users/gaetano/skills"


## 2. Plugin operativi

plugins/
├── <nome>/
│   ├── PLUGIN.md                (frontmatter name/description/trigger/enabled/tools/requires)
│   ├── tool.json                (tool MCP: name/description/procedure/inputSchema)
│   └── <nome>.bas               (riferimento: le procedure NON vengono caricate)


AI PLUGINS CREATE "web-fetch" "Scarica e restituisce il testo di una URL"
AI PLUGINS ADD "web-fetch"
AI PLUGINS ENABLE "web-fetch"     ' → McpToolRegistry.Register(tools da tool.json)
AI PLUGINS DISABLE "web-fetch"
AI PLUGINS REMOVE "web-fetch"
AI PLUGINS REFRESH
AI PLUGINS SET INDEX "~/plugins"  ' persistito su config (ai.json C# / plugins.json C)
AI PLUGINS SET INDEX ""           ' RESET al default (plugins/ della root)
AI PLUGINS MARKETPLACE "https://..." , "dest"
AI PLUGINS LIST

p$ = AI.PLUGINCALL("web-fetch", "https://example.com")
DIM pl AS ARRAY OF STRING = AI.PLUGINSLIST()
x$ = AI.PLUGINSINFO("web-fetch")

AI PATH PLUGIN ADD "/Users/gaetano/plugins"


## 3. MCP

AI MCP SERVER ADD "filesystem", "npx", "-y @modelcontextprotocol/server-filesystem /tmp"
AI MCP SERVER ENABLE "filesystem"
AI MCP SERVER DISABLE "filesystem"
AI MCP SERVER REMOVE "filesystem"
AI MCP SERVER REFRESH
AI MCP SERVER MARKETPLACE GET "nome"
AI MCP REGISTER TOOL "filesystem.read_file", "filesystem"
AI MCP REGISTER TOOL "gmb_stats", "GmbStats", "Statistiche", TRUE, "{""type"":""string""}"
AI MCP REGISTER AGENT "analista"
AI MCP UNREGISTER TOOL "filesystem.read_file"
AI MCP SERVE "stdio"               ' mode = "stdio" | "http" (non un nome di server)
AI MCP SERVE "http", 8765         ' richiede GMBASIC_MCP_API_KEY, altrimenti ritorna 0
AI MCP SERVE "http", 8765, TRUE   ' read-only: rifiuta i tool non dichiarati READONLY
AI MCP STOP

r$ = AI.MCPCALL("filesystem", "read_file", "{""path"":""/tmp/x""}")


## 3ter. Capability server — esporre capability GMBasic come tool MCP (pattern MCP-001)
| Layer | In BASIC | Ruolo |
|---|---|---|
| **Data** | `FUNCTION` sorgente (calcolo/lettura) | da dove vengono i dati |
| **Access** | `FUNCTION` repository | cosa si può consultare, con quali argomenti |
| **MCP** | `AI MCP REGISTER TOOL` + `AI MCP SERVE` | come si espone al client MCP |

' DATA LAYER
FUNCTION SampleData(id AS INTEGER) AS STRING ...
' ACCESS LAYER
FUNCTION GmbStats(id AS INTEGER) AS STRING ...
FUNCTION GmbSavings(id AS INTEGER, cents AS LONG, months AS INTEGER) AS STRING ...
' report multi-fonte = portabile di get_customer_profile di MCP-001
FUNCTION GmbReport(id AS INTEGER, cents AS LONG, months AS INTEGER) AS STRING
    GmbReport = "id=" + STR$(id) + " " + GmbStats(id) + " " + GmbSavings(id, cents, months)
END FUNCTION

SUB Main()
    ' MCP LAYER — 4° argomento TRUE = READONLY (classificazione DICHIARATA, non euristica)
    AI MCP REGISTER TOOL "gmb_stats",   "GmbStats",  "Sample statistics by id", TRUE
    AI MCP REGISTER TOOL "gmb_report",  "GmbReport", "Consolidated multi-source report", TRUE
    PRINT AI.MCPLISTTOOLS()          ' name/description/inputSchema[,outputSchema][,readOnly]
    AI MCP SERVE "http", 8765, TRUE  ' capability server read-only


AI MCP REGISTER TOOL "add_cents", "AddCents", "Somma importi", TRUE, "{""type"":""string""}"
'                        tool        proc        desc            READONLY  outputSchema (opzionale)


### Superficie completa MCP (comandi e funzioni)
| Comando | Note |
|---|---|
| `AI MCP REGISTER TOOL tool, proc [, desc [, readOnly [, outputSchema]]]` | inputSchema derivato dai parametri della `FUNCTION`; `readOnly`=4° arg; `outputSchema`=5° arg (JSON) |
| `AI MCP UNREGISTER TOOL nome` | rimuove il tool dal registro |
| `AI MCP REGISTER AGENT nome` | registra il tool `agent-<nome>` (descrizione `Agent: <nome>`) |
| `AI MCP SERVE "stdio"\| "http", [porta [, readOnly]]` | avvia il server MCP (stdio blocca su stdin; http richiede `GMBASIC_MCP_API_KEY`) |
| `AI MCP STOP` | ferma il server HTTP (no-op per stdio) |
| `AI MCP SERVER ADD/ENABLE/DISABLE/REFRESH/REMOVE …` | server esterni nel `mcp.json` |
| `AI MCP SERVER MARKETPLACE GET "nome"` | dettaglio dal marketplace |

### Verifiche (parità 4 backend)

tools/sh/test_minitests_parity.sh mcp_          # IL + PCodeVM + VM/C
# [storico] il gate C99Native (C#) e' stato rimosso il 2026-10-25: oggi conta la backend C99Native
GMB_MINI_ONLY="mcp_" tools/sh/test_minitests_c99native   # C99Native
tools/test_mcp_e2e.sh                     # client MCP ESTERNO su C99Native + VM/C


### E2E con client MCP esterno, per backend

# IL (ILBackend) — REPL in modalità managed, output pulito con -noconsole
python3 tools/basic/mcp_e2e_client.bas dotnet REPL/bin/Debug/net10.0/REPL.dll -noconsole -m \
        shared/samples/Ai/McpToolServerStdio.bas
# PCodeVM (VM P-Code C# in-process)
python3 tools/basic/mcp_e2e_client.bas dotnet REPL/bin/Debug/net10.0/REPL.dll -noconsole --pcode \
        shared/samples/Ai/McpToolServerStdio.bas
# C99Native: coperto dal gate C99 + dai MiniTest (il runner `--c99` della REPL non passa
#   -I GMBasic/native/gmb_runtime_c/include a cc → "gmb_codegen.h file not found": gap della REPL,
#   non del backend, che invece transpila e gira nel gate)


## 3bis. Connettori (52, `AI CONNECTOR` — discovery automatica)

' Una volta, globale (persistito in config/connectors.json)
AI CONNECTOR LOAD "gmail"
AI CONNECTOR LOAD "filesystem"
AI CONNECTOR SECRET SET "gmail", "api_key", "ya29.demo"

' Ogni nuovo agente li vede senza REGISTER
AI AGENT CREATE "AutoAgent"
' Con LLM: AI.AGENTASKSYNC("AutoAgent","cerca fatture ACME e lista work/")
' → tool_calls: gmail.search {"query":"from:ACME"} → filesystem.list {"path":"work"}
' Equivalente imperativo (ciò che farebbe l'agente):
DIM r1$ AS STRING = AI.CONNECTORCALL("gmail", "search", "{""query"":""from:ACME""}")
DIM r2$ AS STRING = AI.CONNECTORCALL("filesystem", "list", "{""path"":""work""}")

' Discovery per capability (evita 300 tool nel prompt)
DIM found$ AS STRING = AI.CONNECTORSEARCH("mail")   ' → gmail, microsoft-365
PRINT found$

| Comando | Sintassi | Note |
|---|---|---|
| `AI CONNECTOR LOAD` | `AI CONNECTOR LOAD "gmail"` | Dal marketplace 52 → `McpToolRegistry` (`gmail.search`) |
| `AI CONNECTOR SEARCH` | `AI CONNECTOR SEARCH "mail"` / `AI.CONNECTORSEARCH("mail")` | Per capability (`mail`→`gmail`, `db`→`postgres`) |
| `AI CONNECTOR CALL` | `AI CONNECTOR CALL "gmail","search","{...}"` / `AI.CONNECTORCALL` | Via `ConnectorRuntime.cs:116` (`filesystem`/`http`/`sqlite` reali F1, `gmail` con `Bearer` se `AUTH`) |
| `AI CONNECTOR POLICY` | `AI CONNECTOR POLICY SET "gmail","send","approve"` | `HIGH_RISK` → `AgentPermissionService` |
| `AI CONNECTOR SECRET/AUTH` | `SECRET SET` / `AUTH "gmail","{""access_token"":""...""}"` | `Keychain` per-connector (`ConnectorConfig.cs:13`) |

## 4. Distribuire un programma CON le skill/plugin

{
  "skills":  ["board", "financial-analyst"],
  "plugins": ["web-fetch", "browser"],
  "config":  ["mcp.json"]
}

| Cosa | Dove |
|---|---|
| archivio (sviluppo) | `shared/skills`, `shared/plugins` — versionati, piccoli (1,9 MB + 116 KB) |
| indice generato | `<configDir>/SKILLS_PROMPT.md`, `<configDir>/PLUGINS_PROMPT.md` |
| marketplace installati | `~/.gmbasic/skills-marketplaces`, `~/.gmbasic/plugins-marketplaces` |
| bundle di un programma | `<exe>/skills/<nome>`, `<exe>/plugins/<nome>` (solo i dichiarati) |

## 5. File e test
| File | Ruolo |
|---|---|
| `GMBasicSharp/common/AI/SkillIndex.cs` | scan `SKILL.md` → `SKILLS_PROMPT.md` |
| `GMBasicSharp/common/CommonLib/ProgramBundle.cs` | bundle: copia `skills`/`plugins` dichiarati accanto all'eseguibile |
| `GMBasicSharp/common/AI/PluginIndex.cs` + `PluginRegistry.cs` | `Scan/Refresh`, `Enable→McpToolRegistry` |
| `GMBasicSharp/common/AI/Mcp/McpServer.cs` + `McpToolRegistry.cs` | dispatch MCP (`McpServer.cs:80` `ping`/`resources/read`/`prompts/get`) |
| `GMBasicSharp/common/AI/Connectors/ConnectorRegistry.cs:55` + `ConnectorMarketplace.cs:162` + `Connector.AgentBridge.cs:30` | `52` connettori, `RememberTools→McpToolRegistry`, `AgentToolLoop` auto-discovery |
| `GMBasicSharp/common/Parser/Parser.Stmt.Ai.cs:165` + `Parser.Stmt.Connector.cs:224` | `AI CONNECTOR` (ns `AI`) + `AI PATH/SKILLS/PLUGINS/MCP` |
| `GMBasicSharp/common/StdLib/StdLib.AI.cs:124` + `StdLib.Connector.cs:84` | `AI.SKILLSPROMPT/PLUGINCALL/MCPCALL` + `AI.CONNECTORLIST/CALL` (11 proc, ns `AI`) |

## 6. Riferimenti

### ASR-Qwen3-ASR-Guida-Utente

*Fonte: `ASR-Qwen3-ASR-Guida-Utente`*


## 1. Pipeline

WAV → 16 kHz mono → log-mel (128 bin) → Conv2D stem (3×, s2) → Encoder (attn finestrata bidirezionale)
    → proj → prompt ASR → Decoder Qwen3 (GQA + per-head Q/K RMSNorm + NeoX RoPE + SwiGLU, KV cache)
    → greedy → testo


## 2. Superficie BASIC — `ASR.RECOGNIZE(model$, wav$[, outPath$])`

IMPORT ASR
t$ = ASR.RECOGNIZE("<model_dir>", "<file.wav>")               ' trascrizione completa
t$ = ASR.RECOGNIZE("<model_dir>", "<file.wav>", "<out.txt>")  ' STREAMING (transcript live su file)
PRINT t$


## 3. Comando di download del modello

rapidmlx-nextgen pull Qwen/Qwen3-ASR-0.6B --dest /Volumes/Armor5TB/LocalLLM/ai/models --stall-timeout 5 --stall-retries 1
# 1.7B: ... pull Qwen/Qwen3-ASR-1.7B ...


## 3. Uso (harness)

# trascrizione (segmentazione opzionale: 3° arg = secondi/segmento, 0 = nessuna)
gmb_asr_run <model_dir> <file.wav> [seg_sec]

# streaming (chunk 2s + cache finestre encoder + rollback): 4° arg = secondi/chunk (default 2)
gmb_asr_run <model_dir> --stream <file.wav> [chunk_sec]

# encode del tokenizer (debug): ids + round-trip decode
gmb_asr_run <model_dir> --enc "<testo>"


## 4. API C (runtime condiviso)
| Componente | Funzioni |
|---|---|
| Front-end audio | `gmb_asr_wav_load` / `gmb_asr_wav_decode` (WAV 16-bit→mono@16k), `gmb_asr_resample`, `gmb_asr_mel` |
| Kernel | `gmb_asr_linear(_bf16)` (NEON), `gmb_asr_conv2d`, `gmb_asr_layer_norm`, `gmb_asr_rms_norm[_per_head]`, `gmb_asr_{silu,gelu,softmax,swiglu}`, `gmb_asr_attn_bidirectional`, `gmb_asr_attn_causal_gqa`, `gmb_asr_sinusoidal_pe`, `gmb_asr_rope_neox_*`, `gmb_asr_argmax_bf16` |
| Q8 | `gmb_asr_q8_quantize` / `gmb_asr_q8_dequantize` |
| Encoder / Decoder | `gmb_asr_encoder_forward`, `gmb_asr_decoder_forward`, `gmb_asr_kv_init/free` |
| Modello | `gmb_asr_load_model(dir)` / `gmb_asr_model_free` + accessor a config/encoder/decoder |
| Orchestrazione | `gmb_asr_transcribe_segment`, `gmb_asr_transcribe(..., segment_sec, search_sec, ...)` |
| Tokenizer | `gmb_asr_tok_load` / `gmb_asr_tok_decode` / `gmb_asr_tok_encode` |
| Segmentazione | `gmb_asr_segment_split` (confini di silenzio) |
| Streaming bookkeeping | `GmbAsrStream` (cache finestre / eviction / rollback / capping prefisso) |

## 5. Conformance

## 6. Stato e limiti

### Connector-Runtime-Guida-Utente

*Fonte: `Connector-Runtime-Guida-Utente`*


GMBASIC AGENT
      │
  CONNECTOR RUNTIME  ← 8 primitive: CONNECTOR / AUTH / TOOL / RESOURCE / POLICY / APPROVAL / SECRET / SESSION
      │
 ┌────┼───────────────┐
 │    │               │
NATIVE  MCP         OPENAPI   ← un'API uniforme, sotto: Gmail REST, Postgres driver, npx server-filesystem, openapi.json
 │    │               │
DB  Gmail  HTTP    Slack  GitHub


## 1. I 52 connettori (marketplace statico F0)
| Tier | Connettori | Esempio tool |
|------|------------|--------------|
| **T0 fondamentali (10)** | `gmail`, `google-calendar`, `google-drive`, `github`, `slack`, `microsoft-365`, `web`, `http`, `filesystem`, `postgres`, `sqlite` | `gmail.search/read/send`, `github.issues/PR/actions`, `postgres.query/schema` |
| **T1 utili (12)** | `google-sheets`, `google-docs`, `onedrive`, `sharepoint`, `notion`, `linear`, `jira`, `discord`, `telegram`, `microsoft-teams`, `dropbox`, `box` | `notion.search/page.read`, `jira.issues/transitions` |
| **T2 business (10)** | `stripe`, `hubspot`, `salesforce`, `shopify`, `woocommerce`, `zendesk`, `twilio`, `sendgrid`, `docusign`, `calendly` | `stripe.customers/payments`, `hubspot.contacts/deals` — `HIGH_RISK` |
| **T3 DevOps (11)** | `docker`, `kubernetes`, `aws`, `azure`, `google-cloud`, `cloudflare`, `sentry`, `grafana`, `prometheus`, `gitlab`, `bitbucket` | `docker.containers`, `sentry.errors/traces` |
| **T4 AI/Knowledge (9)** | `openai`, `anthropic`, `openrouter`, `huggingface`, `weights-biases`, `qdrant`, `chroma`, `elasticsearch`, `neo4j` | `qdrant.search/insert`, `neo4j.query/traverse` |

## 2. Le 8 primitive (SYSTEM.txt §35)
| Primitive | Ruolo | File |
|-----------|-------|------|
| `CONNECTOR` | Registro + marketplace 52 | `ConnectorRegistry.cs:151` |
| `AUTH` | OAuth2 PKCE + Client ID Metadata (spec 2026-07-28) | `ConnectorAuth.cs:155` |
| `TOOL` | `name/description/inputSchema/outputSchema/permission/risk` | `ConnectorDefinition.cs` |
| `RESOURCE` | `resources/list` + `resources/read` (MCP) | `McpServer.cs:80` |
| `POLICY` | `mail.read` vs `mail.send` + `HIGH_RISK` | `ConnectorPolicy.cs:83` |
| `APPROVAL` | `HUMAN APPROVAL` per `HIGH_RISK` ( `AgentPermissionService.cs:19` ) | `McpServer.cs:228` |
| `SECRET` | `Keychain` / `Secret Service` / `Credential Manager` (per-connector) | `ConnectorConfig.cs` |
| `SESSION` | `state` + `audit log` + `rate limit 120/min` | `ConnectorSession.cs:82` |

## 3. Sintassi BASIC

### 3.1 Comandi (legacy a spazio, senza parentesi — `AGENTS.md` §2.0)

IMPORT AI

' Registro e ciclo di vita
AI CONNECTOR LIST
AI CONNECTOR REGISTER "my-github", "github"
AI CONNECTOR REGISTER "my-pg", "postgres", "{""host"":""localhost""}"
AI CONNECTOR UNREGISTER "my-github"
AI CONNECTOR LOAD "gmail"              ' alias di MARKETPLACE GET + ENABLE
AI CONNECTOR ENABLE "gmail"
AI CONNECTOR DISABLE "gmail"
AI CONNECTOR STATUS "gmail"
AI CONNECTOR REFRESH "gmail"           ' rediscovery tools (MCP initialize + tools/list)

' Tool discovery
AI CONNECTOR SEARCH "mail"             ' cerca per capability/tool/kind
AI CONNECTOR SEARCH "calendar"


### 3.2 Funzioni (con parentesi, ritornano `STRING` JSON — `AGENTS.md` §1)

IMPORT AI

DIM all$ AS STRING = AI.CONNECTORLIST()                 ' registrati
DIM st$ AS STRING  = AI.CONNECTORSTATUS("my-github")    ' singolo + sessione
DIM tools$ AS STRING = AI.CONNECTORTOOLS("my-github")
DIM found$ AS STRING = AI.CONNECTORSEARCH("mail")       ' per capability
DIM market$ AS STRING = AI.CONNECTORMARKETPLACELIST()   ' 52 statici
DIM entry$ AS STRING = AI.CONNECTORMARKETPLACEGET("gmail")
DIM pol$ AS STRING = AI.CONNECTORPOLICYGET("my-stripe", "payments")   ' allow|deny|approve
DIM sec$ AS STRING = AI.CONNECTORSECRETGET("my-stripe", "api_key")
DIM res$ AS STRING = AI.CONNECTORCALL("github", "issues", "{""repo"":""GMBasic""}")
DIM r$ AS STRING   = AI.CONNECTORRESOURCES("gmail")     ' resources/list
DIM p$ AS STRING   = AI.CONNECTORPROMPTS("gmail")       ' prompts/list


## 4. Marketplace e discovery

### 4.1 Marketplace statico F0

IMPORT AI

' 52 connettori — T0 Slack/Notion come esempio
DIM m$ AS STRING = AI.CONNECTORMARKETPLACELIST()
PRINT m$   ' JSON array 52 entry (gmail, github, slack, notion, stripe, aws, qdrant...)

' Dettaglio + auto-registrazione (come MCP MARKETPLACE GET)
DIM g$ AS STRING = AI.CONNECTORMARKETPLACEGET("gmail")
PRINT g$

' Load diretto (registra + abilita)
AI CONNECTOR LOAD "slack"
PRINT AI.CONNECTORSTATUS("slack")


### 4.2 Tool discovery dinamica (SYSTEM.txt §32)

IMPORT AI

' L'agente scopre solo i pertinenti
DIM mailTools$ AS STRING = AI.CONNECTORSEARCH("mail")      ' → gmail, microsoft-365, sendgrid
DIM dbTools$ AS STRING   = AI.CONNECTORSEARCH("db")        ' → postgres, sqlite, supabase
DIM ghTools$ AS STRING   = AI.CONNECTORSEARCH("github")    ' → github, gitlab, bitbucket


## 5. OAuth2 + PKCE (SYSTEM.txt §28-29, MCP spec 2026-07-28)

### 5.1 Flusso PKCE con `include/OAuthClient.inc`

IMPORT AI
IMPORT CRYPTOGRAPHY
IMPORT STRINGS
IMPORT HTTP
IMPORT OAUTH

SUB Main()
    AI CONNECTOR LOAD "gmail"

    ' 1. PKCE verifier 64 char + challenge S256
    DIM verifier$ AS STRING = OAUTH.CreateVerifier(64)
    DIM challenge$ AS STRING = OAUTH.CreateChallenge(verifier$)

    ' 2. Authorization URL (Auth Code + PKCE)
    DIM authUrl$ AS STRING = OAUTH.AuthUrl( _
        "https://accounts.google.com/o/oauth2/v2/auth", _


### 5.2 ApiKey (alternativa a OAuth)

IMPORT AI
AI CONNECTOR SECRET SET "github", "api_key", "ghp_demo123"
DIM k$ AS STRING = AI.CONNECTORSECRETGET("github", "api_key")
' McpClient inietta "Bearer ghp_demo123" se non c'è OAuth token


## 6. Policy, approval e secret (SYSTEM.txt §26-27, §19 HIGH_RISK)

IMPORT AI

AI CONNECTOR REGISTER "my-stripe", "stripe"
AI CONNECTOR POLICY SET "my-stripe", "customers", "allow"    ' READ → LOW
AI CONNECTOR POLICY SET "my-stripe", "payments", "approve"   ' WRITE → MEDIUM/HIGH → human approval
AI CONNECTOR POLICY SET "my-stripe", "refunds", "deny"       ' DESTRUCTIVE

DIM p$ AS STRING = AI.CONNECTORPOLICYGET("my-stripe", "payments")
PRINT p$   ' approve

' Chiamata con gate: YOLO → Allow, Sandbox → Deny, Supervised → Approve (richiede UI)
' McpServer.cs:228 su MCP remoto: Sandbox→deny, Supervised→"needs human approval (not available over MCP)"
DIM ok$ AS STRING = AI.CONNECTORCALL("my-stripe", "customers", "{}")
PRINT ok$


IMPORT AI
AI CONNECTOR SECRET SET "my-stripe", "api_key", "sk_test_123"
DIM v$ AS STRING = AI.CONNECTORSECRETGET("my-stripe", "api_key")


## 7. Risorse, prompt, ping (estensioni MCP)

IMPORT AI

AI CONNECTOR LOAD "gmail"
DIM res$ AS STRING = AI.CONNECTORRESOURCES("gmail")   ' [{uri:"connector://gmail/search", name:"search"}]
DIM prom$ AS STRING = AI.CONNECTORPROMPTS("gmail")    ' [{name:"chat-with-Analista"}] (da AgentServer)
PRINT res$
PRINT prom$


## 8. Esempi pratici per dominio

### 8.1 GitHub (T0 — repository/issues/PR/actions)

IMPORT AI
SUB Main()
    AI CONNECTOR LOAD "github"
    AI CONNECTOR SECRET SET "github", "api_key", "ghp_demo"
    AI CONNECTOR AUTH "github", "{""access_token"":""g_pat""}"

    DIM repo$ AS STRING = AI.CONNECTORCALL("github", "repository", "{""name"":""c-vision/GMBasic""}")
    DIM issues$ AS STRING = AI.CONNECTORCALL("github", "issues", "{""repo"":""GMBasic"",""state"":""open""}")
    DIM pr$ AS STRING = AI.CONNECTORCALL("github", "pull_requests", "{""repo"":""GMBasic""}")

    AI CONNECTOR POLICY SET "github", "git_push", "approve"   ' HIGH_RISK → human approval
END SUB


### 8.2 Database + HTTP + Filesystem (T0 — API comune)

IMPORT AI

AI CONNECTOR LOAD "postgres"
AI CONNECTOR SECRET SET "postgres", "conn_string", "Host=localhost;Database=hotel"
DIM rows$ AS STRING = AI.CONNECTORCALL("postgres", "query", "{""sql"":""SELECT * FROM rooms LIMIT 2""}")
DIM schema$ AS STRING = AI.CONNECTORCALL("postgres", "schema", "{""db"":""hotel""}")

AI CONNECTOR LOAD "http"
DIM html$ AS STRING = AI.CONNECTORCALL("http", "get", "{""url"":""https://example.com""}")

AI CONNECTOR LOAD "filesystem"   ' sandbox: WORKSPACE READ+WRITE, /etc DENY, ~/.ssh DENY
DIM ls$ AS STRING = AI.CONNECTORCALL("filesystem", "list", "{""path"":""/tmp""}")


### 8.3 Composizione (SYSTEM.txt §33 — il valore è la catena)

IMPORT AI

' Gmail attachment → Drive → DB
DIM mail$ AS STRING = AI.CONNECTORCALL("gmail", "search", "{""query"":""contratto hotel""}")
DIM att$ AS STRING  = AI.CONNECTORCALL("gmail", "attachments", "{""id"":""msg123""}")
DIM wr$ AS STRING   = AI.CONNECTORCALL("google-drive", "write", "{""path"":""/contracts/hotel.pdf""}")
DIM ins$ AS STRING  = AI.CONNECTORCALL("postgres", "execute", "{""sql"":""INSERT INTO contracts...""}")

' GitHub flow (SYSTEM.txt §6)
DIM iss$ AS STRING = AI.CONNECTORCALL("github", "issues", "{""repo"":""GMBasic""}")
DIM src$ AS STRING = AI.CONNECTORCALL("github", "repository", "{""name"":""c-vision/GMBasic""}")

' Discovery per toolset (SYSTEM.txt §31-32)
DIM t$ AS STRING = AI.CONNECTORSEARCH("mail")   ' non 300 tool nel prompt, ma discover


## 9. MCP Adapter (SYSTEM.txt §3, §23 — CONNECTOR ≠ MCP SERVER)

GMBASIC AGENT → CONNECTOR API (gmail.search) → OAuth → gmail.googleapis.com  (nativo)
                ↘ MCP ADAPTER → Remote MCP / Local MCP (npx server-filesystem) / Native API


## 10. File e test
| Area | File |
|------|------|
| Catalogo 52 + registry | `GMBasicSharp/common/AI/Connectors/ConnectorMarketplace.cs:162` / `ConnectorRegistry.cs:151` / `ConnectorConfig.cs:134` / `ConnectorDefinition.cs:60` |
| Auth PKCE | `ConnectorAuth.cs:155` + `include/OAuthClient.inc:90` + `shared/samples/Security/OAuth*.bas` |
| Policy/approval | `ConnectorPolicy.cs:83` + `Basic.Agents.AgentPermissionService.cs:19` + `McpServer.cs:228` |
| Session/rate-limit | `ConnectorSession.cs:82` |
| Parser | `Parser.Stmt.Connector.cs:224` + `Parser.Stmt.Ai.cs:165` + `Parser.Statements.cs:666` + `Parser.Utils.cs:73` |
| StdLib | `StdLib.Connector.cs:84` (namespace `AI`) + `StdLib.cs:194` |
| Lessico | `Lexer.Keywords.cs:216` + `Ast/TokenKind.cs:1505` + `Compiler/BuiltInFunction.cs:2003` + `Compiler.Expressions.Functions.cs:544` |
| Runtime IL/PCode | `IlBackendRuntime/ConnectorRuntime.cs:238` + `IlRuntime.cs:8890` + `IlProgramCompiler.BuiltinsData.cs:724` + `IlProgramCompiler.Builtins.cs:1426` + `PCodeProcessor.Builtins.Connector.cs:80` |
| Runtime C (F1) | `GMBasic/native/gmb_runtime_c/include/gmb_connector.h:41` + `src/gmb_connector.c:614` — **registry in-memory F1** (list/status/tools/search/resources/prompts/register/unregister/load/enable/disable/refresh/policy/secret/auth/call) con serializzatori a parità `System.Text.Json` (indentazione 2 spazi, `\uXXXX` per non-ASCII/`&<>'+`) + `CMakeLists.txt` |
| MCP esteso | `McpServer.cs:80` / `McpSession.cs:15` / `McpConstants.cs:23` / `McpClient.cs:197` |
| Sample F0 (7) | `shared/samples/Connectors/ConnectorBasic.bas:79` + `ConnectorMarketplace.bas` + `ConnectorAuth.bas` + `ConnectorPolicy.bas` + `ConnectorGitHub.bas` + `ConnectorDatabaseHttp.bas` + `ConnectorComposability.bas` |
| Sample F1 (3) | `ConnectorPostgresLocal.bas:74` (URL → `Host=` `Npgsql` `AI.csproj:13`) + `AgentConnector*` 4 auto-discovery (`AutoDiscovery`/`FilesystemAuto`/`GmailAuto`/`ComposeAuto`) |
| Sample F2-F4 (3) | `ConnectorT1_SheetsNotionLinear.bas:40` (sheets/notion/linear/jira) + `ConnectorT2_StripeHubspot.bas:30` (stripe/hubspot) + `ConnectorOpenApiPetstore.bas:30` (fetch `swagger.json` → 14 tools, `CreateFromUrl`) |
| Sample full-coverage | `shared/samples/Connectors/ConnectorFullCoverage.bas` — 11 funzioni + tutti i verbi `AI CONNECTOR` |
| MiniTest (18) | `shared/minitests/connector/*.bas` — list/status/tools/search/resources/prompts/call/policy/secret/auth/register/enable/load/refresh/commands/marketplace/marketplaceget/basic |

# [storico] gate C99Native rimosso il 2026-10-25 — oggi: tools/sh/test_minitests_c99native
dotnet test GMBasicSharp/tests/AI.Tests --filter Mcp            # 17/17 PASS
dotnet run --project REPL -- -m shared/samples/Connectors/ConnectorBasic.bas                # IL OK
dotnet run --project REPL -- --pcode shared/samples/Connectors/ConnectorBasic.bas         # PCode OK
cmake -S GMBasic/native/gmb_runtime_c -B /tmp/gmb_runtime_build && cmake --build --target gmb_runtime # 0 Errori

# Parità CONNECTOR (4 backend)
tools/sh/test_minitests_parity.sh connector                                                       # IL/PCode/VMC 18/18
GMB_MINI_ONLY=connector tools/sh/test_minitests_c99native                                      # C99Native
GMB_MINI_ONLY="connector/status.bas" tools/sh/test_minitests_c99native   # C99Native
GMB_SAMPLE_DIR=shared/samples/Connectors GMB_SAMPLE_ONLY=ConnectorFullCoverage dotnet test GMBasicSharp/tests/GMBASIC.IlBackend.Tests -p:GmbEdition=Commercial --filter Sample_Parity_Sweep


## 11. Best practice e limiti F0

## 12. Riferimenti

## 13. Appendice — Esempi per tutti i 52 connettori (53 entry: `web` e `http` separati, spec 52 con `Web/HTTP` unito)

### T0 fondamentali (11)

IMPORT AI
AI CONNECTOR LOAD "gmail"
AI CONNECTOR AUTH "gmail", "{\"access_token\":\"ya29.demo\"}"  ' PKCE via ConnectorAuth.bas
DIM r$ AS STRING = AI.CONNECTORCALL("gmail", "search", '{"query":"from:boss"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "google-calendar"
AI CONNECTOR AUTH "google-calendar", "{\"access_token\":\"ya29.demo\"}"  ' PKCE via ConnectorAuth.bas
DIM r$ AS STRING = AI.CONNECTORCALL("google-calendar", "events", '{}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "google-drive"
AI CONNECTOR AUTH "google-drive", "{\"access_token\":\"ya29.demo\"}"  ' PKCE via ConnectorAuth.bas
DIM r$ AS STRING = AI.CONNECTORCALL("google-drive", "search", '{"query":"contratto hotel"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "github"
AI CONNECTOR AUTH "github", "{\"access_token\":\"ya29.demo\"}"  ' PKCE via ConnectorAuth.bas
DIM r$ AS STRING = AI.CONNECTORCALL("github", "issues", '{"repo":"GMBasic","state":"open"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "slack"
AI CONNECTOR AUTH "slack", "{\"access_token\":\"ya29.demo\"}"  ' PKCE via ConnectorAuth.bas
DIM r$ AS STRING = AI.CONNECTORCALL("slack", "search", '{"query":"deploy failed"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "microsoft-365"
AI CONNECTOR AUTH "microsoft-365", "{\"access_token\":\"ya29.demo\"}"  ' PKCE via ConnectorAuth.bas
DIM r$ AS STRING = AI.CONNECTORCALL("microsoft-365", "mail.search", '{"query":"meeting"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "web"
DIM r$ AS STRING = AI.CONNECTORCALL("web", "search", '{"query":"GMBasic pricing"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "http"
DIM r$ AS STRING = AI.CONNECTORCALL("http", "get", '{"url":"https://example.com"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "filesystem"
DIM r$ AS STRING = AI.CONNECTORCALL("filesystem", "list", '{"path":"/tmp"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "postgres"
DIM r$ AS STRING = AI.CONNECTORCALL("postgres", "query", '{"sql":"SELECT * FROM rooms LIMIT 2"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "sqlite"
DIM r$ AS STRING = AI.CONNECTORCALL("sqlite", "query", '{"sql":"SELECT * FROM notes LIMIT 2"}')
PRINT r$


### T1 utili (12)

IMPORT AI
AI CONNECTOR LOAD "google-sheets"
AI CONNECTOR AUTH "google-sheets", "{\"access_token\":\"ya29.demo\"}"  ' PKCE via ConnectorAuth.bas
DIM r$ AS STRING = AI.CONNECTORCALL("google-sheets", "read", '{"spreadsheetId":"abc","range":"A1:B2"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "google-docs"
AI CONNECTOR AUTH "google-docs", "{\"access_token\":\"ya29.demo\"}"  ' PKCE via ConnectorAuth.bas
DIM r$ AS STRING = AI.CONNECTORCALL("google-docs", "read", '{"documentId":"doc123"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "onedrive"
AI CONNECTOR AUTH "onedrive", "{\"access_token\":\"ya29.demo\"}"  ' PKCE via ConnectorAuth.bas
DIM r$ AS STRING = AI.CONNECTORCALL("onedrive", "list", '{"path":"/"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "sharepoint"
AI CONNECTOR AUTH "sharepoint", "{\"access_token\":\"ya29.demo\"}"  ' PKCE via ConnectorAuth.bas
DIM r$ AS STRING = AI.CONNECTORCALL("sharepoint", "search", '{"query":"budget"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "notion"
AI CONNECTOR AUTH "notion", "{\"access_token\":\"ya29.demo\"}"  ' PKCE via ConnectorAuth.bas
DIM r$ AS STRING = AI.CONNECTORCALL("notion", "search", '{"query":"architettura"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "linear"
AI CONNECTOR SECRET SET "linear", "api_key", "sk_demo"
DIM r$ AS STRING = AI.CONNECTORCALL("linear", "issues", '{"team":"ENG"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "jira"
AI CONNECTOR AUTH "jira", "{\"access_token\":\"ya29.demo\"}"  ' PKCE via ConnectorAuth.bas
DIM r$ AS STRING = AI.CONNECTORCALL("jira", "issues", '{"project":"GMB"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "discord"
AI CONNECTOR AUTH "discord", "{\"access_token\":\"ya29.demo\"}"  ' PKCE via ConnectorAuth.bas
DIM r$ AS STRING = AI.CONNECTORCALL("discord", "search", '{"channel":"general"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "telegram"
AI CONNECTOR SECRET SET "telegram", "api_key", "sk_demo"
DIM r$ AS STRING = AI.CONNECTORCALL("telegram", "search", '{"chat":"mygroup"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "microsoft-teams"
AI CONNECTOR AUTH "microsoft-teams", "{\"access_token\":\"ya29.demo\"}"  ' PKCE via ConnectorAuth.bas
DIM r$ AS STRING = AI.CONNECTORCALL("microsoft-teams", "messages", '{"channel":"general"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "dropbox"
AI CONNECTOR AUTH "dropbox", "{\"access_token\":\"ya29.demo\"}"  ' PKCE via ConnectorAuth.bas
DIM r$ AS STRING = AI.CONNECTORCALL("dropbox", "list", '{"path":""}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "box"
AI CONNECTOR AUTH "box", "{\"access_token\":\"ya29.demo\"}"  ' PKCE via ConnectorAuth.bas
DIM r$ AS STRING = AI.CONNECTORCALL("box", "list", '{"path":"/"}')
PRINT r$


### T2 business (10)

IMPORT AI
AI CONNECTOR LOAD "stripe"
AI CONNECTOR SECRET SET "stripe", "api_key", "sk_demo"
DIM r$ AS STRING = AI.CONNECTORCALL("stripe", "customers", '{}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "hubspot"
AI CONNECTOR AUTH "hubspot", "{\"access_token\":\"ya29.demo\"}"  ' PKCE via ConnectorAuth.bas
DIM r$ AS STRING = AI.CONNECTORCALL("hubspot", "contacts", '{}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "salesforce"
AI CONNECTOR AUTH "salesforce", "{\"access_token\":\"ya29.demo\"}"  ' PKCE via ConnectorAuth.bas
DIM r$ AS STRING = AI.CONNECTORCALL("salesforce", "contacts", '{}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "shopify"
AI CONNECTOR SECRET SET "shopify", "api_key", "sk_demo"
DIM r$ AS STRING = AI.CONNECTORCALL("shopify", "products", '{}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "woocommerce"
AI CONNECTOR SECRET SET "woocommerce", "api_key", "sk_demo"
DIM r$ AS STRING = AI.CONNECTORCALL("woocommerce", "products", '{}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "zendesk"
AI CONNECTOR AUTH "zendesk", "{\"access_token\":\"ya29.demo\"}"  ' PKCE via ConnectorAuth.bas
DIM r$ AS STRING = AI.CONNECTORCALL("zendesk", "tickets", '{}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "twilio"
AI CONNECTOR SECRET SET "twilio", "api_key", "sk_demo"
DIM r$ AS STRING = AI.CONNECTORCALL("twilio", "messages", '{}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "sendgrid"
AI CONNECTOR SECRET SET "sendgrid", "api_key", "sk_demo"
DIM r$ AS STRING = AI.CONNECTORCALL("sendgrid", "send", '{"to":"a@b.com","subject":"hi"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "docusign"
AI CONNECTOR AUTH "docusign", "{\"access_token\":\"ya29.demo\"}"  ' PKCE via ConnectorAuth.bas
DIM r$ AS STRING = AI.CONNECTORCALL("docusign", "envelopes", '{}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "calendly"
AI CONNECTOR AUTH "calendly", "{\"access_token\":\"ya29.demo\"}"  ' PKCE via ConnectorAuth.bas
DIM r$ AS STRING = AI.CONNECTORCALL("calendly", "events", '{}')
PRINT r$


### T3 DevOps (11)

IMPORT AI
AI CONNECTOR LOAD "docker"
DIM r$ AS STRING = AI.CONNECTORCALL("docker", "containers", '{}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "kubernetes"
DIM r$ AS STRING = AI.CONNECTORCALL("kubernetes", "pods", '{}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "aws"
AI CONNECTOR SECRET SET "aws", "api_key", "sk_demo"
DIM r$ AS STRING = AI.CONNECTORCALL("aws", "s3.list", '{"bucket":"my-bucket"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "azure"
AI CONNECTOR AUTH "azure", "{\"access_token\":\"ya29.demo\"}"  ' PKCE via ConnectorAuth.bas
DIM r$ AS STRING = AI.CONNECTORCALL("azure", "storage.list", '{}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "google-cloud"
AI CONNECTOR AUTH "google-cloud", "{\"access_token\":\"ya29.demo\"}"  ' PKCE via ConnectorAuth.bas
DIM r$ AS STRING = AI.CONNECTORCALL("google-cloud", "storage.list", '{}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "cloudflare"
AI CONNECTOR SECRET SET "cloudflare", "api_key", "sk_demo"
DIM r$ AS STRING = AI.CONNECTORCALL("cloudflare", "zones", '{}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "sentry"
AI CONNECTOR SECRET SET "sentry", "api_key", "sk_demo"
DIM r$ AS STRING = AI.CONNECTORCALL("sentry", "errors", '{}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "grafana"
AI CONNECTOR SECRET SET "grafana", "api_key", "sk_demo"
DIM r$ AS STRING = AI.CONNECTORCALL("grafana", "dashboards", '{}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "prometheus"
DIM r$ AS STRING = AI.CONNECTORCALL("prometheus", "query", '{"query":"up"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "gitlab"
AI CONNECTOR AUTH "gitlab", "{\"access_token\":\"ya29.demo\"}"  ' PKCE via ConnectorAuth.bas
DIM r$ AS STRING = AI.CONNECTORCALL("gitlab", "repositories", '{}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "bitbucket"
AI CONNECTOR AUTH "bitbucket", "{\"access_token\":\"ya29.demo\"}"  ' PKCE via ConnectorAuth.bas
DIM r$ AS STRING = AI.CONNECTORCALL("bitbucket", "repositories", '{}')
PRINT r$


### T4 AI/Knowledge (9)

IMPORT AI
AI CONNECTOR LOAD "openai"
AI CONNECTOR SECRET SET "openai", "api_key", "sk_demo"
DIM r$ AS STRING = AI.CONNECTORCALL("openai", "chat", '{"prompt":"hello"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "anthropic"
AI CONNECTOR SECRET SET "anthropic", "api_key", "sk_demo"
DIM r$ AS STRING = AI.CONNECTORCALL("anthropic", "chat", '{"prompt":"hello"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "openrouter"
AI CONNECTOR SECRET SET "openrouter", "api_key", "sk_demo"
DIM r$ AS STRING = AI.CONNECTORCALL("openrouter", "chat", '{"prompt":"hello"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "huggingface"
AI CONNECTOR SECRET SET "huggingface", "api_key", "sk_demo"
DIM r$ AS STRING = AI.CONNECTORCALL("huggingface", "models", '{}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "weights-biases"
AI CONNECTOR SECRET SET "weights-biases", "api_key", "sk_demo"
DIM r$ AS STRING = AI.CONNECTORCALL("weights-biases", "runs", '{}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "qdrant"
DIM r$ AS STRING = AI.CONNECTORCALL("qdrant", "search", '{"collection":"notes","query":"hotel"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "chroma"
DIM r$ AS STRING = AI.CONNECTORCALL("chroma", "search", '{"collection":"docs","query":"test"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "elasticsearch"
DIM r$ AS STRING = AI.CONNECTORCALL("elasticsearch", "search", '{"index":"logs","query":"error"}')
PRINT r$


IMPORT AI
AI CONNECTOR LOAD "neo4j"
DIM r$ AS STRING = AI.CONNECTORCALL("neo4j", "query", '{"cypher":"MATCH (n) RETURN n LIMIT 2"}')
PRINT r$


<!-- END SURFACE -->



<!-- BEGIN DOMAIN TABLE -->
## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **AI.ACTIVE(x1)** — `PRINT "Attivo: "; AI.ACTIVE(%CHAT)` (shared/samples/AiLLM/Qwen3_5_27B_Claude_4_6_Opus_Distilled_MLX_6bit.bas:17)
- **AI.AGENTASKFILE(x1, x2)** — `PRINT "ASKFILE: ", AI.AGENTASKFILE("assistant", "fattura-inesistente.pdf")` (shared/samples/Ai/AiAgentAskFile.bas:13)
- **AI.AGENTASKSYNC(x1, x2)** — `ris$ = AI.AGENTASKSYNC("ag", "In una riga: qual e' la capitale dell'Italia?")` (shared/samples/AiLLM/Qwen3_5_27B_Claude_4_6_Opus_Distilled_MLX_6bit.bas:25)
- **AI.AGENTCODE(x1)** — `PRINT "r="; AI.AGENTCODE("scrivi un programma che stampa 42")` (shared/samples/Ai/AgentCodeParity.bas:3)
- **AI.AGENTEVENTS()** — `ev$ = AI.AGENTEVENTS()` (shared/samples/AiLLM/AgentStructuredToolCalling.bas:65)
- **AI.AGENTFIX(x1)** — `PRINT "agentfix:     "; AI.AGENTFIX("errore fittizio")` (shared/samples/Ai/AiVaultNotesCoverage.bas:49)
- **AI.AGENTGET(x1, x2)** — `PRINT "thinking="; AI.AGENTGET("ExtA", "thinking")` (shared/samples/Team/TeamExtensions.bas:21)
- **AI.AGENTLIST()** — `PRINT "--- AI.AGENTLIST() ---"` (shared/samples/Ai/AgentOperatingCenter.bas:28)
- **AI.AGENTPAUSE(x1)** — `DIM p = AI.AGENTPAUSE("demoAgent")` (shared/samples/Ai/AiAgentLifecycle.bas:40)
- **AI.AGENTQUEUECLEAR(x1)** — `DIM c = AI.AGENTQUEUECLEAR("queueAgent")` (shared/samples/Ai/AiAgentLifecycle.bas:54)
- **AI.AGENTQUEUESTATS(x1)** — `DIM qs$ = AI.AGENTQUEUESTATS("demoAgent")` (shared/samples/Ai/AiAgentLifecycle.bas:36)
- **AI.AGENTRECEIVE(x1)** — `DIM rx$ = AI.AGENTRECEIVE("demoAgent")` (shared/samples/Ai/AiAgentLifecycle.bas:31)
- **AI.AGENTRESUME(x1)** — `DIM r = AI.AGENTRESUME("demoAgent")` (shared/samples/Ai/AiAgentLifecycle.bas:44)
- **AI.AGENTRUN(x1, x2)** — `answer$ = AI.AGENTRUN("agent1", "Determina il titolo della pagina https://example.com usando gli str…` (shared/samples/AiLLM/AiAgentRunCua.bas:20)
- **AI.AGENTRUNCODE(x1, x2)** — `answer = AI.AGENTRUNCODE("code-agent", _` (shared/samples/Ai/AiAgentCode.bas:20)
- **AI.AGENTRUNGRAPH(x1, x2)** — `answer$ = AI.AGENTRUNGRAPH("graph-agent", _` (shared/samples/graph/GraphAgentAsk.bas:26)
- **AI.AGENTRUNRAG(x1, x2)** — `answer$ = AI.AGENTRUNRAG("rag-agent", _` (shared/samples/graph/RagAgentAsk.bas:18)
- **AI.AGENTSEND(x1, x2, x3)** — `DIM ok = AI.AGENTSEND("demoAgent", "task1", "test message")` (shared/samples/Ai/AiAgentLifecycle.bas:29)
- **AI.AGENTSTATUS(x1)** — `PRINT "STATUS="; AI.AGENTSTATUS("ToolAgent")` (shared/samples/AiLLM/AgentStructuredToolCalling.bas:63)
- **AI.AGENTSTOP(x1)** — `DIM s = AI.AGENTSTOP("demoAgent")` (shared/samples/Ai/AiAgentLifecycle.bas:48)
- **AI.AGENTSWARM(x1, x2, x3)** — `PRINT AI.AGENTSWARM(4, "worker_", "riassumi in una riga: cos'e' una coda FIFO", 2)` (shared/samples/Ai/AgentSwarmAsync.bas:10)
- **AI.ANALYZEIMAGE(x1)** — `BrowserAnalyze = AI.ANALYZEIMAGE(outPath, prompt)` (shared/samples/Browser/BrowserE8.bas:146)
- **AI.ARTIFACTGET(x1)** — `PRINT "riferimento relazione: "; AI.ARTIFACTGET("relazione")` (shared/samples/Ai/AiArtifactStore.bas:10)
- **AI.ARTIFACTLINK(x1, x2)** — `ok = AI.ARTIFACTLINK("repository", "https://example.com/repo")` (shared/samples/Ai/AiArtifactStore.bas:9)
- **AI.ARTIFACTLIST()** — `PRINT AI.ARTIFACTLIST()` (shared/samples/Ai/AiArtifactStore.bas:11)
- **AI.ARTIFACTPUT(x1, x2)** — `ok = AI.ARTIFACTPUT("relazione", "/tmp/relazione.md")` (shared/samples/Ai/AiArtifactStore.bas:8)
- **AI.ARTIFACTREMOVE(x1)** — `ok = AI.ARTIFACTREMOVE("repository")` (shared/samples/Ai/AiArtifactStore.bas:12)
- **AI.ASK(x1, x2, x3)** — `answer = AI.ASK("vecdb", "kb", "What is semantic retrieval?", 3)` (shared/samples/Ai/SqlVecSearch.bas:20)
- **AI.ASKCHAIN(x1, x2)** — `PRINT "askchain_ok:  "; IIF(LEN(AI.ASKCHAIN("x", "x", "x")) >= 0, 1, 0)` (shared/samples/Ai/AiFullSurfaceCoverage.bas:39)
- **AI.ASKMULTI(x1)** — `PRINT "ask="; AI.ASKMULTI("x", 1, 1, 1)` (shared/samples/Ai/AiMissingCoverage.bas:9)
- **AI.BACKLINKS(x1, x2)** — `PRINT "backlinks:   "; AI.BACKLINKS("sb", "altra.md")` (shared/samples/Ai/AiSecondBrainVault.bas:44)
- **AI.BLACKBOARDREAD(x1)** — `tot = tot + VAL(AI.BLACKBOARDREAD("bb_" + STR(i)))` (shared/samples/Ai/BlackboardWorkersE2E.bas:36)
- **AI.BLACKBOARDWAIT(x1)** — `r = AI.BLACKBOARDWAIT("bbwait-mini-key", 200)` (shared/minitests/misc/blackboard_wait_goroutine.bas:11)
- **AI.BLACKBOARDWRITE(x1, x2)** — `d = AI.BLACKBOARDWRITE("bb_" + STR(id), STR(v))` (shared/samples/Ai/BlackboardWorkersE2E.bas:20)
- **AI.BRAINDETACH(x1)** — `DIM bd = AI.BRAINDETACH("default")` (shared/samples/Ai/AiAgentLifecycle.bas:80)
- **AI.BRAININFO(x1)** — `PRINT "braininfo:    "; AI.BRAININFO("default")` (shared/samples/Ai/AiVaultNotesCoverage.bas:42)
- **AI.BRAININSTALL(x1)** — `PRINT "BRAININSTALL(manuale)   = "; AI.BRAININSTALL("manuale")` (shared/samples/Ai/AiBrainGaps.bas:13)
- **AI.BRAINLIST()** — `PRINT "brainlist:    "; AI.BRAINLIST()` (shared/samples/Ai/AiVaultNotesCoverage.bas:41)
- **AI.BRAINUNINSTALL(x1)** — `PRINT "BRAINUNINSTALL(manuale) = "; AI.BRAINUNINSTALL("manuale")` (shared/samples/Ai/AiBrainGaps.bas:14)
- **AI.BRAINUSE(x1)** — `DIM bu = AI.BRAINUSE("default")` (shared/samples/Ai/AiAgentLifecycle.bas:76)
- **AI.BRAINUSED()** — `DIM used$ = AI.BRAINUSED()` (shared/samples/graph/FinanceBrainGraph.bas:22)
- **AI.BRAINVAULTBUILD(x1)** — `PRINT "BUILD:", AI.BRAINVAULTBUILD(WORK$)` (shared/samples/graph/BrainVault.bas:18)
- **AI.BRAINVAULTQUERY(x1)** — `PRINT "QUERY:", AI.BRAINVAULTQUERY("ricerca vettoriale su collezioni sqlite", 3)` (shared/samples/graph/BrainVault.bas:22)
- **AI.BRAINVAULTWRITE(x1)** — `PRINT "WRITE:", AI.BRAINVAULTWRITE("scrivi una funzione che somma due numeri", 3)` (shared/samples/graph/BrainVault.bas:26)
- **AI.CONNECTORCALL(x1, x2, x3)** — `DIM mail$ AS STRING = AI.CONNECTORCALL("gmail", "search", "{""query"":""contratto hotel""}")` (shared/samples/Connectors/ConnectorComposability.bas:20)
- **AI.CONNECTORLIST()** — `PRINT "LIST len="; LEN(AI.CONNECTORLIST())` (shared/samples/Connectors/ConnectorFullCoverage.bas:12)
- **AI.CONNECTORMARKETPLACEGET(x1)** — `PRINT "MARKETPLACEGET len="; LEN(AI.CONNECTORMARKETPLACEGET("gmail"))` (shared/samples/Connectors/ConnectorFullCoverage.bas:14)
- **AI.CONNECTORMARKETPLACELIST()** — `PRINT "MARKETPLACELIST len="; LEN(AI.CONNECTORMARKETPLACELIST())` (shared/samples/Connectors/ConnectorFullCoverage.bas:13)
- **AI.CONNECTORPOLICYGET(x1, x2)** — `DIM pol$ AS STRING = AI.CONNECTORPOLICYGET("gmail", "send")` (shared/samples/Connectors/AgentConnectorGmailAuto.bas:40)
- **AI.CONNECTORPROMPTS(x1)** — `PRINT "PROMPTS="; AI.CONNECTORPROMPTS("gmail")` (shared/samples/Connectors/ConnectorFullCoverage.bas:19)
- **AI.CONNECTORRESOURCES(x1)** — `PRINT "RESOURCES len="; LEN(AI.CONNECTORRESOURCES("gmail"))` (shared/samples/Connectors/ConnectorFullCoverage.bas:18)
- **AI.CONNECTORSEARCH(x1)** — `DIM mailTools$ AS STRING = AI.CONNECTORSEARCH("mail")` (shared/samples/Connectors/ConnectorComposability.bas:42)
- **AI.CONNECTORSECRETGET(x1, x2)** — `PRINT "SECRETGET="; AI.CONNECTORSECRETGET("gmail", "api_key")` (shared/samples/Connectors/ConnectorFullCoverage.bas:21)
- **AI.CONNECTORSTATUS(x1)** — `PRINT "STATUS len="; LEN(AI.CONNECTORSTATUS("gmail"))` (shared/samples/Connectors/ConnectorFullCoverage.bas:15)
- **AI.CONNECTORTOOLS(x1)** — `PRINT "TOOLS len="; LEN(AI.CONNECTORTOOLS("gmail"))` (shared/samples/Connectors/ConnectorFullCoverage.bas:16)
- **AI.CATEGORYAUTO(x1)** — `PRINT "cat_auto:     "; AI.CATEGORYAUTO(1)` (shared/samples/Ai/AiFullSurfaceCoverage.bas:23)
- **AI.CATEGORYFALLBACK(x1, x2)** — `PRINT "cat_fallback: "; AI.CATEGORYFALLBACK("studio", "fixture-completion")` (shared/samples/Ai/AiFullSurfaceCoverage.bas:24)
- **AI.CATEGORYLIST()** — `PRINT "cat_list_ok:  "; IIF(LEN(AI.CATEGORYLIST()) >= 0, 1, 0)` (shared/samples/Ai/AiFullSurfaceCoverage.bas:21)
- **AI.CATEGORYPICK(x1)** — `PRINT "cat_pick_ok:  "; IIF(LEN(AI.CATEGORYPICK("studio")) >= 0, 1, 0)` (shared/samples/Ai/AiFullSurfaceCoverage.bas:22)
- **AI.CATEGORYSET(x1, x2)** — `PRINT "cat_set:      "; AI.CATEGORYSET("studio", "fixture-completion")` (shared/samples/Ai/AiFullSurfaceCoverage.bas:20)
- **AI.CHANNELCREATE(x1)** — `DIM ch = AI.CHANNELCREATE("demoChan")` (shared/samples/Ai/AiAgentLifecycle.bas:60)
- **AI.CHANNELPUBLISH(x1, x2)** — `DIM pub = AI.CHANNELPUBLISH("demoChan", "hello from channel")` (shared/samples/Ai/AiAgentLifecycle.bas:68)
- **AI.CHANNELSUBSCRIBE(x1, x2)** — `DIM chanSub = AI.CHANNELSUBSCRIBE("demoChan", "demoAgent")` (shared/samples/Ai/AiAgentLifecycle.bas:64)
- **AI.CHANNELUNSUBSCRIBE(x1, x2)** — `DIM chanUnsub = AI.CHANNELUNSUBSCRIBE("demoChan", "demoAgent")` (shared/samples/Ai/AiAgentLifecycle.bas:72)
- **AI.CHECKPOINT(x1, x2)** — `ok = AI.CHECKPOINT("migrazione", "task=compiler;step=3")` (shared/samples/Ai/AiCheckpointRecovery.bas:8)
- **AI.CHECKPOINTCLEAR()** — `ok = AI.CHECKPOINTCLEAR()` (shared/samples/Ai/AiCheckpointRecovery.bas:11)
- **AI.CHECKPOINTLIST()** — `PRINT "checkpoint: "; AI.CHECKPOINTLIST()` (shared/samples/Ai/AiCheckpointRecovery.bas:10)
- **AI.CONFIGURE(x1, x2, x3)** — `AI.CONFIGURE("my-ollama", "url", "http://ollama.local:11434")` (shared/samples/Ai/ProviderConfigure.bas:31)
- **AI.CONSISTENCY(x1)** — `print "--- 3. AI.CONSISTENCY (richiede provider %CHAT REALE) ---"` (shared/samples/Ai/SelfConsistencyDemo.bas:83)
- **AI.CONSISTENCYDETAIL()** — `r_a$ = AI.CONSISTENCYDETAIL$()` (shared/samples/Ai/SelfConsistencyDemo.bas:27)
- **AI.CONTEXTBUDGET(x1)** — `PRINT "1. Budget impostato: " + STR$(AI.CONTEXTBUDGET("worker1"))` (shared/samples/Ai/ContextEngineDemo.bas:10)
- **AI.CONTEXTHASH(x1)** — `PRINT "hash = "; AI.CONTEXTHASH$(agent)` (shared/samples/Ai/ContextBuilderDemo.bas:44)
- **AI.CONTEXTOVERFLOW(x1)** — `PRINT "4. Overflow: " + STR$(AI.CONTEXTOVERFLOW("worker1"))` (shared/samples/Ai/ContextEngineDemo.bas:28)
- **AI.CONTEXTUSED(x1)** — `PRINT "3. Caratteri usati: " + STR$(AI.CONTEXTUSED("worker1"))` (shared/samples/Ai/ContextEngineDemo.bas:25)
- **AI.CONTEXTVALID(x1, x2)** — `PRINT "valid(rev) = "; AI.CONTEXTVALID(agent, AI.CONTEXTHASH$(agent))` (shared/samples/Ai/ContextBuilderDemo.bas:45)
- **AI.CONTEXTVIEW(x1)** — `DIM view$ = AI.CONTEXTVIEW$("worker1")` (shared/samples/Ai/ContextEngineDemo.bas:21)
- **AI.CONTEXTVIEWDEBUG(x1)** — `DIM dbg$ = AI.CONTEXTVIEWDEBUG$("worker1")` (shared/samples/Ai/ContextEngineDemo.bas:31)
- **AI.COUNT(x1, x2)** — `PRINT "Indexed 3 products. Total embeddings: "; AI.COUNT(":memory:", "products")` (shared/samples/Databases/VectorDbBasic.bas:23)
- **AI.CREDITS(x1)** — `PRINT "Crediti per "; nome; ": "; AI.CREDITS(nome)` (shared/samples/Ai/AiEmbeddedRegisterRouting.bas:63)
- **AI.DISCOVER(x1, x2, x3, x4)** — `_a80 = AI.Discover("", "", "", "", 0)` (shared/minitests/ai/ai_all.bas:95)
- **AI.DOCEXTRACT(x1, x2)** — `PRINT "EXTRACT: ", AI.DOCEXTRACT(inv, schema)` (shared/samples/Ai/AiDocumentFields.bas:27)
- **AI.DOCFIELDS(x1)** — `PRINT "FIELDS: ", AI.DOCFIELDS(inv)` (shared/samples/Ai/AiDocFields.bas:15)
- **AI.EMBED(x1)** — `dims = AI.EMBED("hello world", 8)` (shared/samples/Ai/SqlVecSearch.bas:24)
- **AI.ENRICH(x1, x2)** — `PRINT "enrich:      "; IIF(LEN(AI.ENRICH("x", "x")) >= 0, 1, 0)` (shared/samples/Ai/AiFullSurfaceCoverage.bas:49)
- **AI.ENSURECOLLECTION(x1, x2)** — `PRINT "AI.ENSURECOLLECTION..."` (shared/samples/Databases/AiLoadDocs.bas:16)
- **AI.FAIL(x1)** — `AI.FAIL("primary-emb")` (shared/samples/Ai/ProviderFailover.bas:37)
- **AI.FLEETSTATS()** — `PRINT "fleet: "; AI.FLEETSTATS()` (shared/samples/Ai/AiFleetMetrics.bas:14)
- **AI.GENTEST(x1, x2, x3)** — `_a88 = AI.GenTest("", "", "")` (shared/minitests/ai/ai_all.bas:103)
- **AI.GETBASE(x1)** — `url = AI.GETBASE("openrouter")` (shared/samples/Ai/AiProviders.bas:63)
- **AI.GETDEFAULT()** — `def = AI.GETDEFAULT()` (shared/samples/Ai/AiProviders.bas:78)
- **AI.GETKEY(x1)** — `ok = AI.GETKEY("openrouter")` (shared/samples/Ai/AiProviders.bas:28)
- **AI.GETMODEL(x1)** — `model = AI.GETMODEL("openrouter")` (shared/samples/Ai/AiProviders.bas:39)
- **AI.GETTEMP(x1)** — `temp = AI.GETTEMP("openrouter")` (shared/samples/Ai/AiProviders.bas:51)
- **AI.GOALAGENT(x1)** — `PRINT "agente: "; AI.GOALAGENT("goal-agent")` (shared/samples/Ai/AiGoalContinuation.bas:12)
- **AI.GOALCLEAR()** — `ok = AI.GOALCLEAR()` (shared/samples/Ai/AiGoalContinuation.bas:17)
- **AI.GOALGET()** — `PRINT "obiettivo: "; AI.GOALGET()` (shared/samples/Ai/AiGoalContinuation.bas:13)
- **AI.GOALSET(x1)** — `ok = AI.GOALSET("portare a termine la migrazione del compilatore")` (shared/samples/Ai/AiGoalContinuation.bas:11)
- **AI.GOALSTATUS()** — `PRINT "stato: "; AI.GOALSTATUS()` (shared/samples/Ai/AiGoalContinuation.bas:16)
- **AI.GOALTICK()** — `PRINT "continuazione 1: "; AI.GOALTICK()` (shared/samples/Ai/AiGoalContinuation.bas:14)
- **AI.HANDOFF(x1, x2)** — `ok = AI.HANDOFF("refactor", "coder")` (shared/samples/Ai/AiHandoff.bas:8)
- **AI.HANDOFFCLEAR()** — `ok = AI.HANDOFFCLEAR()` (shared/samples/Ai/AiHandoff.bas:14)
- **AI.HANDOFFCOMPLETE(x1, x2)** — `ok = AI.HANDOFFCOMPLETE("refactor", "completato")` (shared/samples/Ai/AiHandoff.bas:11)
- **AI.HANDOFFLIST()** — `PRINT AI.HANDOFFLIST()` (shared/samples/Ai/AiHandoff.bas:13)
- **AI.HANDOFFSTATUS(x1)** — `PRINT "refactor: "; AI.HANDOFFSTATUS("refactor")` (shared/samples/Ai/AiHandoff.bas:10)
- **AI.HASHEDIT(x1, x2, x3)** — `PRINT AI.HASHEDIT(s, "2", "  return 42")` (shared/samples/Ai/AiHashline.bas:12)
- **AI.HASHLINE(x1)** — `PRINT AI.HASHLINE(s)` (shared/samples/Ai/AiHashline.bas:9)
- **AI.HASHVERIFY(x1, x2)** — `PRINT "verifica riga 2: "; AI.HASHVERIFY(s, "2")` (shared/samples/Ai/AiHashline.bas:10)
- **AI.HEALTHCHECKINTERVAL()** — `PRINT "Intervallo di default: "; AI.HEALTHCHECKINTERVAL(); " minuti"` (shared/samples/Ai/ProviderStatus.bas:46)
- **AI.HFREPO()** — `PRINT "hfrepo:      "; IIF(LEN(AI.HFREPO()) >= 0, 1, 0)` (shared/samples/Ai/AiFullSurfaceCoverage.bas:93)
- **AI.HOOKEVENTS()** — `PRINT "hook_events:  "; IIF(LEN(AI.HOOKEVENTS()) >= 0, 1, 0)` (shared/samples/Ai/AiFullSurfaceCoverage.bas:30)
- **AI.HOOKFIRE(x1)** — `PRINT "hook_fire_ok: "; IIF(LEN(AI.HOOKFIRE("pre", "{}")) >= 0, 1, 0)` (shared/samples/Ai/AiFullSurfaceCoverage.bas:29)
- **AI.HOOKLIST()** — `PRINT "hook_list_ok: "; IIF(LEN(AI.HOOKLIST()) >= 0, 1, 0)` (shared/samples/Ai/AiFullSurfaceCoverage.bas:28)
- **AI.HOOKREGISTER(x1, x2)** — `PRINT "hook_reg:     "; AI.HOOKREGISTER("pre", "Handler")` (shared/samples/Ai/AiFullSurfaceCoverage.bas:27)
- **AI.HOOKUNREGISTER(x1, x2)** — `PRINT "hook_unreg:   "; AI.HOOKUNREGISTER("pre", "Handler")` (shared/samples/Ai/AiFullSurfaceCoverage.bas:31)
- **AI.IMAGECLASSIFY(x1, x2)** — `PRINT "CLASSIFY: ", AI.IMAGECLASSIFY("immagine-inesistente.png", "fattura,ricevuta,documento")` (shared/samples/Ai/AiImageClassify.bas:12)
- **AI.IMAGECOMPARE(x1, x2)** — `PRINT "COMPARE: ", AI.IMAGECOMPARE(a, a)          ' 100` (shared/samples/Ai/AiImageCompare.bas:15)
- **AI.IMAGECONVERT(x1, x2)** — `ok = AI.IMAGECONVERT(src, "work/ai_info_copy.png")` (shared/samples/Ai/AiImageConvert.bas:15)
- **AI.IMAGECROP(x1, x2, x3, x4, x5, x6)** — `r = AI.IMAGECROP(src, "work/ai_image_crop.png", 1, 1, 2, 2)` (shared/samples/Ai/AiImageManipulation.bas:27)
- **AI.IMAGEDETECT(x1)** — `PRINT "DETECT: ", AI.IMAGEDETECT("immagine-inesistente.png")` (shared/samples/Ai/AiImageDetect.bas:13)
- **AI.IMAGEEMBED(x1)** — `DIM e() = AI.IMAGEEMBED(src)` (shared/samples/Ai/AiImageEmbed.bas:16)
- **AI.IMAGEINFO(x1)** — `PRINT "INFO:  ", AI.IMAGEINFO("work/ai_video_frame.png")` (shared/samples/Ai/AiVideoFrame.bas:17)
- **AI.IMAGERESIZE(x1, x2, x3, x4)** — `r = AI.IMAGERESIZE(src, "work/ai_image_small.png", 8, 6)` (shared/samples/Ai/AiImageManipulation.bas:22)
- **AI.INDEXMETA(x1, x2, x3, x4, x5)** — `PRINT "indexMeta:"; AI.INDEXMETA(db, coll, "doc2", "il cane corre nel parco", "{""lang"":""it""}")` (shared/samples/Ai/AiVectorFunctions.bas:18)
- **AI.INDEXTEXT(x1, x2, x3, x4)** — `PRINT "index:    "; AI.INDEXTEXT(db, coll, "doc1", "il gatto dorme sul divano")` (shared/samples/Ai/AiVectorFunctions.bas:17)
- **AI.INFO(x1)** — `info = AI.INFO("openrouter")` (shared/samples/Ai/AiProviders.bas:70)
- **AI.INGESTWEB(x1, x2)** — `PRINT "ingestweb:    "; AI.INGESTWEB("cov", "https://example.com")` (shared/samples/Ai/AiVaultNotesCoverage.bas:56)
- **AI.KGADDEDGE(x1, x2)** — `AI.KGADDEDGE("GMBasic", "Lexer")` (shared/samples/Ai/KnowledgeGraphDemo.bas:28)
- **AI.KGADDNODE(x1)** — `AI.KGADDNODE("GMBasic")` (shared/samples/Ai/KnowledgeGraphDemo.bas:19)
- **AI.KGQUERY(x1)** — `PRINT AI.KGQUERY("Parser")` (shared/samples/Ai/KnowledgeGraphDemo.bas:39)
- **AI.LEXICALENTROPY(x1)** — `print "--- 1. AI.LEXICALENTROPY (metrica deterministica) ---"` (shared/samples/Ai/SelfConsistencyDemo.bas:21)
- **AI.LINKDANGLING(x1)** — `PRINT "linkDangling:"; AI.LINKDANGLING("sb")` (shared/samples/Ai/AiSecondBrainVault.bas:46)
- **AI.LINKJSON(x1)** — `PRINT "linkJson:    "; IIF(LEN(AI.LINKJSON("sb")) > 0, 1, 0)` (shared/samples/Ai/AiSecondBrainVault.bas:45)
- **AI.LINKSOUT(x1, x2)** — `PRINT "linksOut:    "; AI.LINKSOUT("sb", "intro.md")` (shared/samples/Ai/AiSecondBrainVault.bas:43)
- **AI.LIST(x1)** — `PRINT "Provider completion: "; AI.LIST("completion")` (shared/samples/AiLLM/TwoLocalModels.bas:38)
- **AI.LISTCOLLECTIONS(x1)** — `PRINT AI.LISTCOLLECTIONS(":memory:")` (shared/samples/Databases/VectorDbBasic.bas:44)
- **AI.LOAD(x1)** — `AI.LOAD("/tmp/gmbasic-providers.json")` (shared/samples/Ai/ProviderConfigure.bas:80)
- **AI.LOADDOCUMENTS(x1, x2, x3)** — `n = AI.LOADDOCUMENTS("kb", "docs", docPath$, "*.txt", "")` (shared/samples/Ai/AgentIngestionDemo.bas:37)
- **AI.MCPCALL(x1, x2, x3)** — `DIM ris$ = AI.MCPCALL("files", "read_file", "{""path"":""/tmp/a.txt""}")` (shared/samples/Ai/McpServerListClient.bas:53)
- **AI.MCPLISTTOOLS()** — `PRINT "Tool calc_sum registrato: " + AI.MCPLISTTOOLS()` (shared/samples/AiLLM/AgentStructuredToolCalling.bas:31)
- **AI.MCPLISTTOOLSSERVER(x1)** — `PRINT AI.MCPLISTTOOLSSERVER("files")` (shared/samples/Ai/McpServerListClient.bas:36)
- **AI.MCPPROMPTS()** — `PRINT "PROMPTS()   = "; AI.MCPPROMPTS()` (shared/samples/Ai/McpUncoveredCoverage.bas:42)
- **AI.MCPRESOURCES()** — `PRINT "RESOURCES() = "; AI.MCPRESOURCES()` (shared/samples/Ai/McpUncoveredCoverage.bas:41)
- **AI.MCPSERVERLIST()** — `PRINT "Lista:"; AI.MCPSERVERLIST()` (shared/samples/Ai/McpServerListClient.bas:31)
- **AI.MCPSERVERMARKETPLACELIST()** — `PRINT AI.MCPSERVERMARKETPLACELIST()` (shared/samples/Ai/McpServerListClient.bas:20)
- **AI.MCPSERVERSTATUS(x1)** — `PRINT "Stato: "; AI.MCPSERVERSTATUS("files")` (shared/samples/Ai/McpServerListClient.bas:30)
- **AI.MEMORYASK(x1)** — `PRINT "MemoryAsk: ["; AI.MEMORYASK("query-inesistente-xyz-123", 3); "]"` (shared/samples/Ai/AgentMemoryAndPermissions.bas:14)
- **AI.MEMORYCONSOLIDATE()** — `PRINT "memconsol:    "; AI.MEMORYCONSOLIDATE()` (shared/samples/Ai/AiVaultNotesCoverage.bas:50)
- **AI.MEMORYRANK(x1)** — `PRINT "memoryrank:   "; AI.MEMORYRANK("query")` (shared/samples/Ai/AiVaultNotesCoverage.bas:46)
- **AI.METRICGET(x1, x2)** — `PRINT "token researcher: "; AI.METRICGET("researcher", "tokens")` (shared/samples/Ai/AiFleetMetrics.bas:12)
- **AI.METRICLIST()** — `PRINT "per-agente: "; AI.METRICLIST()` (shared/samples/Ai/AiFleetMetrics.bas:13)
- **AI.METRICRECORD(x1, x2, x3)** — `ok = AI.METRICRECORD("researcher", "tokens", 1200)` (shared/samples/Ai/AiFleetMetrics.bas:8)
- **AI.METRICRESET()** — `ok = AI.METRICRESET()` (shared/samples/Ai/AiFleetMetrics.bas:15)
- **AI.NOTEEXISTS(x1, x2)** — `c.NoteOk = AI.NOTEEXISTS("demo", "nota.md")` (shared/samples/Ai/BrainMultiBrain.bas:77)
- **AI.NOTEFOLDER(x1, x2)** — `PRINT "noteFolder:  "; AI.NOTEFOLDER("sb", "intro.md")` (shared/samples/Ai/AiSecondBrainVault.bas:38)
- **AI.NOTEGET(x1, x2)** — `PRINT "noteGet:     "; AI.NOTEGET("sb", "intro.md")` (shared/samples/Ai/AiSecondBrainVault.bas:33)
- **AI.NOTELINK(x1, x2, x3)** — `PRINT "notelink:     "; AI.NOTELINK("cov", "a.md", "b.md", "vedi")` (shared/samples/Ai/AiVaultNotesCoverage.bas:36)
- **AI.NOTESETALIAS(x1, x2, x3, x4)** — `PRINT "notesetalias: "; AI.NOTESETALIAS("cov", "a.md", "b.md", "alias1")` (shared/samples/Ai/AiVaultNotesCoverage.bas:37)
- **AI.NOTETAGS(x1, x2)** — `PRINT "noteTags:    "; AI.NOTETAGS("sb", "intro.md")` (shared/samples/Ai/AiSecondBrainVault.bas:35)
- **AI.NOTETITLE(x1, x2)** — `PRINT "noteTitle:   "; AI.NOTETITLE("sb", "intro.md")` (shared/samples/Ai/AiSecondBrainVault.bas:34)
- **AI.NOTEUNLINK(x1, x2, x3)** — `PRINT "noteunlink:   "; AI.NOTEUNLINK("cov", "a.md", "b.md")` (shared/samples/Ai/AiVaultNotesCoverage.bas:38)
- **AI.NOTEWORDCOUNT(x1, x2)** — `PRINT "noteWords:   "; AI.NOTEWORDCOUNT("sb", "intro.md")` (shared/samples/Ai/AiSecondBrainVault.bas:36)
- **AI.OCR(x1)** — `desc = AI.OCR("foto.jpg")` (shared/samples/Ai/AiVision.bas:15)
- **AI.OBSERVATION(x1)** — `DIM o$ = AI.OBSERVATION$(lid)` (shared/samples/Ai/ObserveDemo.bas:30)
- **AI.OBSERVATIONCOUNT()** — `DIM n = AI.OBSERVATIONCOUNT()` (shared/samples/Ai/ObserveDemo.bas:26)
- **AI.OBSERVATIONFILE(x1)** — `PRINT "   path=" + AI.OBSERVATIONFILE$(fid)` (shared/samples/Ai/ObserveDemo.bas:15)
- **AI.OBSERVATIONHASH(x1)** — `PRINT "   hash=" + AI.OBSERVATIONHASH$(fid)` (shared/samples/Ai/ObserveDemo.bas:14)
- **AI.OBSERVATIONLATEST()** — `DIM latest$ = AI.OBSERVATIONLATEST$()` (shared/samples/Ai/ObserveDemo.bas:22)
- **AI.OBSERVATIONLIST()** — `DIM l$ = AI.OBSERVATIONLIST$()` (shared/samples/Ai/ObserveDemo.bas:34)
- **AI.OPTION(x1)** — `dir = AI.Option("moe_swap_dir")` (shared/samples/Ai/AiOptions.bas:22)
- **AI.ORCHLOAD()** — `PRINT "orchload:         "; IIF(AI.ORCHLOAD("/tmp/gmb_orch_test.db") >= -1, 1, 0)` (shared/samples/Ai/AiFullSurfaceCoverage.bas:59)
- **AI.ORCHSAVE()** — `n = AI.ORCHSAVE("work/orchestration.db")` (shared/samples/Ai/AiOrchestrationPersistence.bas:14)
- **AI.PLANADD(x1, x2)** — `PRINT "planadd:       "; IIF(LEN(CSTR(AI.PLANADD("x", "x"))) >= 0, 1, 0)` (shared/samples/Ai/AiFullSurfaceCoverage.bas:88)
- **AI.PLANAPPROVE(x1)** — `PRINT "planapprove:   "; IIF(LEN(CSTR(AI.PLANAPPROVE("x"))) >= 0, 1, 0)` (shared/samples/Ai/AiFullSurfaceCoverage.bas:89)
- **AI.PLANCLEAR()** — `PRINT "planclear:    "; AI.PLANCLEAR()` (shared/samples/Ai/AiVaultNotesCoverage.bas:51)
- **AI.PLANDEFINE(x1, x2)** — `PRINT "plandefine:    "; IIF(LEN(CSTR(AI.PLANDEFINE("x", "x"))) >= 0, 1, 0)` (shared/samples/Ai/AiFullSurfaceCoverage.bas:84)
- **AI.PLANLIST()** — `PRINT "planlist:      "; IIF(LEN(CSTR(AI.PLANLIST())) >= 0, 1, 0)` (shared/samples/Ai/AiFullSurfaceCoverage.bas:87)
- **AI.PLANRUN(x1)** — `PRINT "planrun:       "; IIF(LEN(CSTR(AI.PLANRUN("x"))) >= 0, 1, 0)` (shared/samples/Ai/AiFullSurfaceCoverage.bas:86)
- **AI.PLANSTATUS(x1)** — `PRINT "planstatus:    "; IIF(LEN(CSTR(AI.PLANSTATUS("x"))) >= 0, 1, 0)` (shared/samples/Ai/AiFullSurfaceCoverage.bas:85)
- **AI.PLUGINCALL(x1)** — `s$ = AI.PLUGINCALL("web-fetch", "https://example.com")` (shared/samples/Ai/AiPlugins.bas:38)
- **AI.PLUGINLIST()** — `PRINT "REGISTRO="; AI.PLUGINLIST()` (shared/samples/Ai/PluginsLifecycle.bas:48)
- **AI.PLUGINSINFO(x1)** — `s$ = AI.PLUGINSINFO("web-fetch")` (shared/samples/Ai/AiPlugins.bas:30)
- **AI.PLUGINSLIST()** — `PRINT "plugins: "; IIF(LEN(AI.PLUGINSLIST()) > 0, 1, 0)` (shared/samples/Ai/SecondBrainFullCoverage.bas:52)
- **AI.PRIORITY(x1)** — `PRINT "  "; afterList(i); " (priority "; AI.PRIORITY(afterList(i)); ")"` (shared/samples/Ai/ProviderConfigure.bas:45)
- **AI.PROVIDERS()** — `list = AI.PROVIDERS()` (shared/samples/Ai/AiProviders.bas:19)
- **AI.PURGEDEPRECATED(x1, x2)** — `_a185 = AI.PurgeDeprecated("", "")` (shared/minitests/ai/ai_all.bas:200)
- **AI.QAANSWER(x1, x2)** — `PRINT "qaanswer:     "; AI.QAANSWER("domanda", "default")` (shared/samples/Ai/AiVaultNotesCoverage.bas:48)
- **AI.RAGCONTEXT(x1, x2, x3)** — `ctx = AI.RAGCONTEXT("vecdb", "kb", "semantic retrieval", 3)` (shared/samples/Ai/SqlVecSearch.bas:17)
- **AI.RAGCONTEXTMULTI(x1)** — `PRINT "ragmulti:     "; AI.RAGCONTEXTMULTI("query")` (shared/samples/Ai/AiVaultNotesCoverage.bas:47)
- **AI.REGISTER(x1, x2, x3, x4)** — `ok = AI.REGISTER("embedding", "emb-backup", "fixture", "")` (shared/samples/Ai/VectorProviderFailover.bas:13)
- **AI.REMOVESOURCE(x1, x2, x3)** — `PRINT "remove:   "; AI.REMOVESOURCE(db, coll, "doc1")` (shared/samples/Ai/AiVectorFunctions.bas:31)
- **AI.REPLAYCLEAR()** — `ok = AI.REPLAYCLEAR()` (shared/samples/Ai/AiSessionReplay.bas:17)
- **AI.REPLAYGET(x1)** — `esiti = AI.REPLAYGET("rfc_replay#auto")` (shared/samples/Ai/RuntimeFullCoverage.bas:137)
- **AI.REPLAYLIST()** — `PRINT "sessioni: "; AI.REPLAYLIST()` (shared/samples/Ai/AiSessionReplay.bas:16)
- **AI.REPLAYRECORD(x1)** — `rs = AI.REPLAYRECORD("1 + 1")` (shared/samples/Ai/RuntimeFullCoverage.bas:129)
- **AI.REPLAYRUN(x1, x2)** — `PRINT "replay: "; AI.REPLAYRUN("sessione-demo", "sessione-copia")` (shared/samples/Ai/AiSessionReplay.bas:14)
- **AI.REPLAYSTART(x1)** — `rs = AI.REPLAYSTART("rfc_replay")` (shared/samples/Ai/RuntimeFullCoverage.bas:128)
- **AI.REPLAYSTOP()** — `rs = AI.REPLAYSTOP()` (shared/samples/Ai/RuntimeFullCoverage.bas:133)
- **AI.RERANK(x1, x2, x3)** — `PRINT "rerank:   "; LEFT(AI.RERANK("gatto", hits, 3), 200)` (shared/samples/Ai/AiVectorFunctions.bas:25)
- **AI.RESETFAILED(x1)** — `AI.RESETFAILED("")` (shared/samples/Ai/ProviderFailover.bas:56)
- **AI.RESOURCEAVAILABLE(x1)** — `PRINT "model_slots disponibili: "; AI.RESOURCEAVAILABLE("model_slots")` (shared/samples/Ai/AiResourceManager.bas:15)
- **AI.RESOURCEGET(x1)** — `PRINT "max_agents="; AI.RESOURCEGET("max_agents")` (shared/samples/Ai/AiResourceManager.bas:13)
- **AI.RESOURCELIST()** — `PRINT AI.RESOURCELIST()` (shared/samples/Ai/AiResourceManager.bas:16)
- **AI.RESOURCESET(x1, x2)** — `ok = AI.RESOURCESET("max_agents", "4")` (shared/samples/Ai/AiResourceManager.bas:9)
- **AI.RESOURCESWARMSIZE(x1)** — `PRINT "swarm(10)="; AI.RESOURCESWARMSIZE(10)` (shared/samples/Ai/AiResourceManager.bas:14)
- **AI.RESUME(x1)** — `PRINT "riprende: "; AI.RESUME("migrazione")` (shared/samples/Ai/AiCheckpointRecovery.bas:9)
- **AI.ROTATE(x1)** — `rotated$ = AI.ROTATE("embedding")` (shared/samples/Ai/VectorProviderFailover.bas:16)
- **AI.RULESADD(x1, x2)** — `ok = AI.RULESADD("/progetto", "Regole globali del progetto")` (shared/samples/Ai/AiRulesHierarchy.bas:9)
- **AI.RULESCLEAR()** — `ok = AI.RULESCLEAR()` (shared/samples/Ai/AiRulesHierarchy.bas:16)
- **AI.RULESFOR(x1)** — `PRINT AI.RULESFOR("/progetto/src/main.bas")` (shared/samples/Ai/AiRulesHierarchy.bas:13)
- **AI.RULESLIST()** — `PRINT "scope registrati: "; AI.RULESLIST()` (shared/samples/Ai/AiRulesHierarchy.bas:14)
- **AI.RULESREMOVE(x1)** — `ok = AI.RULESREMOVE("/progetto/src")` (shared/samples/Ai/AiRulesHierarchy.bas:15)
- **AI.RUNHEALTHCHECK()** — `PRINT "--- Esecuzione AI.RUNHEALTHCHECK() ---"` (shared/samples/Ai/ProviderHealthCheck.bas:33)
- **AI.SAVE(x1)** — `ok = AI.SAVE("providers-demo.json")` (shared/samples/Ai/ProviderConfig.bas:17)
- **AI.SEARCH(x1, x2, x3, x4)** — `DIM json$ = AI.SEARCH("JsonDb", "json_test", "casa vendita", 5)` (shared/samples/Databases/VectorSearchJsonParsing.bas:24)
- **AI.SEARCHMULTI(x1)** — `DIM r1$ = AI.SEARCHMULTI("come funziona il settlement BSP tra agenzie e compagnie", 3, "wiki", NULL,…` (shared/samples/Ai/AirlineBrainRag.bas:31)
- **AI.SETACTIVE(x1, x2)** — `AI.SETACTIVE("embedding", "fixture-embedding")` (shared/samples/Ai/AiDbAdmin.bas:20)
- **AI.SETCREDITS(x1, x2)** — `_a217 = AI.SetCredits("", 0)` (shared/minitests/ai/ai_all.bas:232)
- **AI.SETPRIORITY(x1, x2)** — `AI.SETPRIORITY("healthy-emb", 100)` (shared/samples/Ai/ProviderHealthCheck.bas:15)
- **AI.SETSTATUS(x1, x2)** — `AI.SETSTATUS("blocked-emb", "BLOCKED")` (shared/samples/Ai/ProviderHealthCheck.bas:28)
- **AI.SETWEIGHT(x1, x2)** — `AI.SETWEIGHT("heavy", 3)` (shared/samples/Ai/ProviderWeightedRotation.bas:19)
- **AI.SHAREDECRYPT(x1, x2)** — `PRINT "DECRYPT:", AI.SHAREDECRYPT(CIPHER$, KEY$)` (shared/samples/graph/Provenance.bas:36)
- **AI.SHAREENCRYPT(x1, x2)** — `CIPHER$ = AI.SHAREENCRYPT(CONTENT$, KEY$)` (shared/samples/graph/Provenance.bas:29)
- **AI.SIGNNOTE(x1, x2)** — `SIG$ = AI.SIGNNOTE(CONTENT$, KEY$)` (shared/samples/graph/Provenance.bas:14)
- **AI.SKILLSLIST()** — `PRINT "skills:  "; IIF(LEN(AI.SKILLSLIST()) > 0, 1, 0)` (shared/samples/Ai/SecondBrainFullCoverage.bas:50)
- **AI.SKILLSPATHS()** — `PRINT "spaths:  "; IIF(LEN(AI.SKILLSPATHS()) > 0, 1, 0)` (shared/samples/Ai/SecondBrainFullCoverage.bas:51)
- **AI.SKILLSPROMPT()** — `PRINT "skillsprompt:"; IIF(LEN(AI.SKILLSPROMPT()) > 0, 1, 0)` (shared/samples/Ai/AiFullSurfaceCoverage.bas:92)
- **AI.STARTHEALTHCHECK(x1)** — `AI.STARTHEALTHCHECK(10)  ' 0 = usa il default (10 minuti)` (shared/samples/Ai/ProviderStatus.bas:47)
- **AI.STATELOAD(x1, x2)** — `PRINT "2. manage_exit=" + AI.STATELOAD("mgr", "manage_exit")` (shared/samples/Ai/WorkerManage.bas:33)
- **AI.STATUS(x1)** — `PRINT "Stato "; nome; ": "; AI.STATUS(nome)` (shared/samples/Ai/AiEmbeddedRegisterRouting.bas:78)
- **AI.STOPHEALTHCHECK()** — `AI.STOPHEALTHCHECK()` (shared/samples/Ai/ProviderStatus.bas:51)
- **AI.SUGGESTTAGS(x1)** — `PRINT "suggesttags:  "; AI.SUGGESTTAGS("testo di prova con parole chiave")` (shared/samples/Ai/AiVaultNotesCoverage.bas:45)
- **AI.SYNCFOLDER(x1, x2)** — `PRINT "syncfolder:   "; AI.SYNCFOLDER("cov", v)` (shared/samples/Ai/AiVaultNotesCoverage.bas:55)
- **AI.SYNTHESIZE(x1, x2)** — `ok = AI.SYNTHESIZE("Buongiorno, come posso aiutarla?", "work/ai_tts.wav")` (shared/samples/Ai/AiSynthesize.bas:13)
- **AI.TASKASSIGN(x1, x2)** — `PRINT "taskassign:    "; IIF(LEN(CSTR(AI.TASKASSIGN("x", "x"))) >= 0, 1, 0)` (shared/samples/Ai/AiFullSurfaceCoverage.bas:83)
- **AI.TASKCLEAR()** — `PRINT "taskclear:    "; AI.TASKCLEAR()` (shared/samples/Ai/AiVaultNotesCoverage.bas:52)
- **AI.TASKDEFINE(x1, x2)** — `PRINT "taskdefine:    "; IIF(LEN(CSTR(AI.TASKDEFINE("x", "x"))) >= 0, 1, 0)` (shared/samples/Ai/AiFullSurfaceCoverage.bas:78)
- **AI.TASKDEPENDS(x1, x2)** — `PRINT "taskdepends:   "; IIF(LEN(CSTR(AI.TASKDEPENDS("x", "x"))) >= 0, 1, 0)` (shared/samples/Ai/AiFullSurfaceCoverage.bas:82)
- **AI.TASKLIST()** — `PRINT "tasklist:      "; IIF(LEN(CSTR(AI.TASKLIST())) >= 0, 1, 0)` (shared/samples/Ai/AiFullSurfaceCoverage.bas:81)
- **AI.TASKMETRICS(x1)** — `PRINT "metrics = "; AI.TASKMETRICS$(agent)` (shared/samples/Ai/ContextBuilderDemo.bas:54)
- **AI.TASKRUN()** — `PRINT "taskrun:       "; IIF(LEN(CSTR(AI.TASKRUN())) >= 0, 1, 0)` (shared/samples/Ai/AiFullSurfaceCoverage.bas:80)
- **AI.TASKSTATUS(x1)** — `PRINT "taskstatus:    "; IIF(LEN(CSTR(AI.TASKSTATUS("x"))) >= 0, 1, 0)` (shared/samples/Ai/AiFullSurfaceCoverage.bas:79)
- **AI.TEAMAGENTS(x1)** — `PRINT "-- AI.TEAMAGENTS --"` (shared/samples/Team/TeamFullCoverage.bas:17)
- **AI.TEAMLATENT(x1)** — `PRINT "latent="; AI.TEAMLATENT("Squadra")` (shared/samples/Team/TeamRecursiveText.bas:27)
- **AI.TEAMLINKINFO(x1)** — `link$ = AI.TEAMLINKINFO("T")` (shared/samples/Team/TeamTrain.bas:33)
- **AI.TEAMLIST()** — `PRINT "-- AI.TEAMLIST --"` (shared/samples/Team/TeamFullCoverage.bas:14)
- **AI.TEAMONSTREAM(x1)** — `DIM streamOn = AI.TEAMONSTREAM("on-member")` (shared/samples/Team/TeamRunStream.bas:27)
- **AI.TEAMRECURSION(x1)** — `PRINT "recursion="; AI.TEAMRECURSION("Squadra")` (shared/samples/Team/TeamRecursiveText.bas:26)
- **AI.TEAMRUNSTART(x1, x2)** — `DIM start$ AS STRING = AI.TEAMRUNSTART("team-inesistente", "task")` (shared/samples/Team/TeamRunStream.bas:17)
- **AI.TEAMRUNWAIT(x1)** — `DIM wait$ AS STRING = AI.TEAMRUNWAIT("handle-inesistente")` (shared/samples/Team/TeamRunStream.bas:21)
- **AI.TEAMRUNTIMESTATS(x1)** — `stats$ = AI.TEAMRUNTIMESTATS("Squadra")` (shared/samples/Team/TeamRecursiveText.bas:30)
- **AI.TEAMTRAINSTATUS(x1)** — `status$ = AI.TEAMTRAINSTATUS("T")` (shared/samples/Team/TeamTrain.bas:30)
- **AI.TEST(x1, x2)** — `DIM t1$ = AI.TEST("embedding", "fixture-embedding")` (shared/samples/Ai/AiDbAdmin.bas:25)
- **AI.TRANSCRIBE(x1)** — `text = AI.TRANSCRIBE("riunione.mp3")` (shared/samples/Ai/AiTranscribe.bas:15)
- **AI.UNREGISTER(x1)** — `DIM removed = AI.UNREGISTER("my-ollama")` (shared/samples/Ai/ProviderConfigure.bas:66)
- **AI.VAULTADDFOLDER(x1, x2)** — `PRINT "addfolder:    "; AI.VAULTADDFOLDER("cov", v, 1)` (shared/samples/Ai/AiVaultNotesCoverage.bas:29)
- **AI.VAULTCOUNT(x1)** — `PRINT "vaultcount:   "; AI.VAULTCOUNT("cov")` (shared/samples/Ai/AiVaultNotesCoverage.bas:24)
- **AI.VAULTFOLDERS(x1)** — `PRINT "vaultfolders: "; AI.VAULTFOLDERS("cov")` (shared/samples/Ai/AiVaultNotesCoverage.bas:25)
- **AI.VAULTINFO(x1)** — `DIM vinfo AS STRING = AI.VAULTINFO("cov")` (shared/samples/Ai/AiVaultNotesCoverage.bas:22)
- **AI.VAULTLIST()** — `PRINT "vaultlist:    "; AI.VAULTLIST()` (shared/samples/Ai/AiVaultNotesCoverage.bas:26)
- **AI.VAULTPOLICY(x1)** — `PRINT "POLICY default:", AI.VAULTPOLICY("watchv")` (shared/samples/graph/VaultWatch.bas:18)
- **AI.VAULTREMOVEFOLDER(x1, x2)** — `PRINT "rmfolder:     "; AI.VAULTREMOVEFOLDER("cov", v)` (shared/samples/Ai/AiVaultNotesCoverage.bas:31)
- **AI.VAULTSETFOLDERPROPS(x1, x2)** — `PRINT "setprops:     "; AI.VAULTSETFOLDERPROPS("cov", v, 1)` (shared/samples/Ai/AiVaultNotesCoverage.bas:30)
- **AI.VAULTWATCH(x1, x2)** — `PRINT "vaultwatch:  "; IIF(AI.VAULTWATCH("x", "x") >= -1, 1, 0)` (shared/samples/Ai/AiFullSurfaceCoverage.bas:91)
- **AI.VERIFYNOTE(x1, x2, x3)** — `PRINT "VERIFY OK:", AI.VERIFYNOTE(CONTENT$, KEY$, SIG$)` (shared/samples/graph/Provenance.bas:18)
- **AI.VIDEOFRAME(x1, x2, x3)** — `ok = AI.VIDEOFRAME(v, 1, "work/ai_video_frame.png")` (shared/samples/Ai/AiVideoFrame.bas:15)
- **AI.VIDEOINFO(x1)** — `PRINT "INFO: ", AI.VIDEOINFO(v)` (shared/samples/Ai/AiVideoInfo.bas:15)
- **AI.VIDEOSNAPSHOT(x1, x2)** — `ok = AI.VIDEOSNAPSHOT(v, "work/ai_video_snapshot.png")` (shared/samples/Ai/AiVideoSnapshot.bas:15)
- **AI.WEIGHT(x1)** — `PRINT "heavy peso="; AI.WEIGHT("heavy"); "  light peso="; AI.WEIGHT("light")` (shared/samples/Ai/ProviderWeightedRotation.bas:24)
- **AI.WORKERRESCUED()** — `IF AI.WORKERRESCUED() > 0 THEN PRINT "rescued: si" ELSE PRINT "rescued: no"` (shared/samples/Ai/WorkerLoop.bas:48)
- **AI.WORKERROUNDS()** — `PRINT "6. rounds=" + STR$(AI.WORKERROUNDS())` (shared/samples/Ai/WorkerManage.bas:40)
- **AI.WORKERSPEND()** — `IF AI.WORKERSPEND() > 0 THEN PRINT "spend: si" ELSE PRINT "spend: no"` (shared/samples/Ai/WorkerLoop.bas:49)
- **AI.WORKFLOWLIST()** — `PRINT "workflow_ok:  "; IIF(LEN(AI.WORKFLOWLIST()) >= 0, 1, 0)` (shared/samples/Ai/AiFullSurfaceCoverage.bas:35)
- **AI.WORKFLOWSTATUS(x1)** — `PRINT "   FlowA status: " + AI.WorkflowStatus("FlowA")` (shared/samples/Ai/WebAgentWorkflowsDemo.bas:28)
- **AI.WORKFLOWSTEPS(x1)** — `steps = AI.WorkflowSteps("Pipeline")` (shared/samples/Ai/WorkflowAndAgentDemo.bas:14)
- **ASR.RECOGNIZE(x1, x2)** — `PRINT "=== ASR.RECOGNIZE streaming — transcript live su work/asr_stream.txt ==="` (shared/samples/AiLLM/AsrTranscribe.bas:11)
<!-- END DOMAIN TABLE -->
