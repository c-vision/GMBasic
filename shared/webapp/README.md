# WEBAPP — Agents Operating Center

Cruscotto web dello stack **AI Agents** di GMBasic, scritto **interamente in BASIC**:
un launcher, cinque pagine BSP e due file JS. Nessun C# da toccare, nessun build step.

> **Runtime canonico: C.** `tools/sh/webapp_up.sh start` (default) e
> `gmb run --project shared/webapp` eseguono l'app con il compilatore C (C99Native): un solo
> eseguibile nativo (runtime C + server HTTP + agent API). Il percorso C#
> (`gmbcs run --engine pcode`) resta disponibile come alternativa/parità.

```
shared/webapp/
├── app.bas              launcher: config, seed, due server, proxy /api/*, WebSocket
├── .webapp.toml         configurazione (porte, seed, refresh, rete, percorsi)
├── pages/
│   ├── index.bsp        dashboard 8 tab: Agents · Teams · Log · Workflows · Memory · Brain · Canvas · WASM
│   ├── canvas.bsp       canvas standalone (protocollo WEBCANVAS)
│   └── wasm.bsp         shell del target `.bas → .wasm`
└── static/
    ├── canvas.js        replay dei comandi di disegno
    └── wasm.js          glue per il modulo wasm32
```

## Architettura

| componente | porta | cosa fa |
|---|---|---|
| `AI AGENT SERVE` | **8765** | API JSON del runtime (`native/gmb_runtime_c/src/gmb_agent_http.c`): `/api/agents`, `/api/teams` (+ `/{n}`, POST, `/{n}/agents`, DELETE), `/api/events`, `/api/workflows` (+ `/{n}`, `/{n}/steps`, `/{n}/execute`, `DELETE`), `/api/team/{n}/broadcast`, `/api/agent/{n}/prompt`, `/api/memory` (+ `/search`), `/api/brain/*`, `/api/enrich` |
| `HTTP SERVER` + BSP | **8090** | serve `pages/*.bsp` renderizzate e `static/*`; espone **tutta** `/api/*` come proxy same-origin verso 8765 (niente CORS); endpoint propri della app |
| `HTTP ROUTE /ws/canvas` | 8090 | WebSocket: `WEBCANVAS` spinge frame JSON che `canvas.js` disegna |

### Endpoint propri della app (implementati in `app.bas`)

| Metodo | Path | Cosa fa |
|---|---|---|
| GET | `/api/health` | liveness (`{"ok":true}`) |
| GET | `/api/status` | stato app+runtime: provider attivo, modello, agenti, eventi, uptime, porte, reachability dell'API, contatore proxy |
| GET | `/api/state` | **stato esplicito** per la UI: `agents:[{name,state}]` (idle/working/waiting/error), `inbox:[{state,agent,kind,detail}]`, `counts`; classificazione eventi fatta **una volta, lato server** |
| POST | `/api/agents/{name}/pause` · `/resume` · `/stop` | azioni sulla **coda** dell'agente (`AI.AGENTPAUSE/RESUME/STOP`); `ok:false` = niente in coda |
| POST | `/api/agents` | crea agente `{name, systemPrompt?, permission?, temperature?, model?}` → 201, **409** se esiste |
| PATCH | `/api/agents/{name}` | aggiorna i soli campi presenti (prompt/permessi/temperatura/modello) |
| DELETE | `/api/agents/{name}` | elimina l'agente |
| POST | `/api/agents/{name}/ask` | `{prompt}` → risposta **sincrona** (`AI.AGENTASKSYNC`), senza server di streaming |
| POST | `/api/teams` | crea team `{name, strategy?}` → 201, **409** se esiste |
| POST | `/api/teams/{name}/agents` | aggiunge membro `{agent, role?}` |
| DELETE | `/api/teams/{name}` | elimina il team |

Tutto il resto di `/api/*` è proxy trasparente (metodo, path e query invariati).

### Persistenza (agenti e team)

| cosa | file | quando |
|---|---|---|
| agente | `bin/agents/<nome>.agent.json` | scritto/aggiornato a ogni **create/update**, cancellato al **delete** (`AI AGENT SAVE`) |
| team | `bin/teams/<nome>.team.json` | scritto dopo **create** e **add member**, cancellato al **delete** (endpoint `save`/`load` dell'API) |

All'avvio l'app ricarica tutto: `AI AGENT LOAD` per gli agenti, `POST /api/teams/load`
per i team. **Il seed demo scatta solo se non c'è nessun agente persistito.**
`bin/` è gitignorato (artefatti-dati della app: mai nel sorgente).

## UI (2026-11-02) — idee da `tuios`

`tuios` (terminal window manager) organizza il lavoro per **stato** e per **attenzione**:
rail delle sessioni, Inbox di ciò che aspetta l'umano, command palette, scorciatoie e temi.
La dashboard ha adottato gli stessi pattern, senza nuovi endpoint e senza build step:

| Cosa | Come funziona | Tasto |
|---|---|---|
| **Command palette** | fuzzy su tab, azioni, agenti e team (azioni eseguibili, non solo navigazione) | `Ctrl`/`Cmd`+`K` (o `Ctrl`+`P`) |
| **Rail agenti** | striscia sotto le tab con stato a colpo d'occhio: `working` (verde), `waiting` (ambra), `done` (blu), `error` (rosso); click → seleziona e apre la chat | — |
| **Inbox** | aggrega dagli eventi ciò che richiede attenzione (domande/approvazioni/errori/turni finiti) con Log/Chat per riga e contatore nell'header | `i` |
| **Tema** | `auto` (segue il sistema) → `dark` → `light`; persistito in `localStorage` | `t` |
| **Scorciatoie** | `1…8` tab, `n` nuovo agente, `/` cerca nel log, `r` refresh, `?` aiuto, `Esc` chiude gli overlay | `?` |
| **Notifiche** | opt-in con la Notification API quando cresce il numero di voci in attesa | — |
| **Azioni coda** | nella tabella Agents e nella palette: Pausa/Stop per gli agenti `working`, Riprendi per gli altri (`POST /api/agents/{name}/{pause,resume,stop}`) | — |
| **Filtri salvati** | nel Log: salva/elimina/riapplica filtri (persistiti in `localStorage`) | — |

Persistenza locale: `gmb-webapp-theme`, `gmb-webapp-notify`, `gmb-webapp-tab`.
L'Inbox e lo stato per agente arrivano da **`GET /api/state`** (server-side): la UI non
classifica più il testo degli eventi. La classificazione è una sola, in `app.bas`:
`PermissionRequested` → `waiting`, `PermissionDenied` → `error`, `Asked`/`ToolCalled` →
`working`, il resto `idle`. Se l'endpoint non c'è (app più vecchia) il JS ricade sulla
classificazione locale degli eventi. SSE resta per il Log live; `/api/state` è interrogato al
tick (e con debounce sugli eventi live).

## Engine BSP (motore) — cosa c'è sotto

Le pagine non sono template: sono **compilate ed eseguite**. Il motore BSP ha ora (2026-10-31):

| funzione | effetto | come si attiva |
|---|---|---|
| **ETag + 304** | il browser che ricarica riceve `304 Not Modified` (niente corpo) | **sempre** |
| **gzip** | HTML compresso quando il client lo accetta e il corpo è > 1 KB (~46 KB → ~16 KB) | **sempre** |
| **Hot reload delle dipendenze** | modificare un `.inc` importato (o la pagina) la ricompila alla richiesta successiva, **senza riavviare** | **sempre** |
| **Memoizzazione della resa** | per N ms la pagina non viene rieseguita: si riusa il corpo (ETag incluso) | `bspCacheMs` (0 = off) |
| **Guardia privacy** | la memoizzazione è **disattivata** per le richieste con cookie (contenuto per-utente) | automatica |

Misure (loopback, keep-alive, c=10, PCode): `/index.bsp` da ~190 rps a **~21.000 rps** con
`bspCacheMs = 1000`; `/canvas.bsp` **~56.000 rps**; revalidation `304` ~25.000 rps. Il costo
"a freddo" di una pagina resta ~2 ms: host+VM costano ~0,07 ms, il resto è l'esecuzione della
pagina (query + costruzione dell'HTML).

### Su C99Native (`gmb run`): il compilatore BSP in C è COMPLETO

Il percorso C usa **lo stesso motore VM/C del managed** (dylib `libgmb_bsp`):

1. `gmb build`/`run` precompila ogni `<progetto>/pages/*.bsp` in un chunk A1
   (`gmbchunk`, che accetta direttamente il `.bsp`: BSP → BASIC **con gli IMPORT**
   → bytecode). Se il chunk è più vecchio della pagina viene rigenerato;
2. all'avvio il runtime carica `libgmb_bsp` (dlopen) e le passa la pagina: la **VM/C**
   esegue il chunk, quindi la pagina usa **tutto il linguaggio** — `FUNCTION`/`SUB`
   degli include (`DASHBOARD.*`, `WASMBRIDGE.*`), classi, `SELECT CASE`, ecc.;
3. lo **stato** (agenti/team/workflow/memoria) arriva dall'host tramite l'hook opzionale
   `gmb_state_hook`: la VM vede il registro del processo, non una copia.

**Nessun fallback per il server BSP**: se il chunk A1 o `libgmb_bsp` mancano, la risposta è
un **500 esplicito** ("BSP non compilata", causa su stderr), mai una pagina resa in modo
diverso. `gmb_expr` non è più usato dal server BSP: resta solo per EVAL/`&`, filtri
DB/SPEED e REQUIRE (dove è un valutatore di espressioni a runtime, non un compilatore di
pagine). I `.chunk` sono artefatti di build (gitignored).

### Due motori, UN SOLO stato (agenti/team/workflow/memoria)

La pagina è precompilata in un chunk A1; lo stesso chunk è eseguito da **due motori**:

| motore | dove esegue | velocità (`index.bsp`, 47 KB, cache spenta) |
|---|---|---|
| **managed** | VM P-Code/IL nel processo .NET | ~6,9 ms / 163 rps (c=1) · 179 rps (c=10) |
| **C** (`libgmb_bsp`, P/Invoke) | VM/C in `native/gmb_vm_c` | **~0,7 ms / 1.436 rps (c=1)** · **2.340 rps (c=10)** — **8,8× / 13×** |

I motori sono **allineati**: lo stato di agenti/team/workflow/memoria è **uno solo**. La VM/C lo
legge e lo scrive attraverso un **hook opzionale** (`gmb_state_hook.h`, registrato dal client C#
via P/Invoke) che delega alle **stesse API** che usano le gambe managed
(`AgentServer`/`TeamServer`/`WorkflowServer`/`AgentMemoryStore`). Senza client managed
(C99Native, `gmb_vm_host`) l'hook è assente e la VM/C usa il proprio registro: comportamento
invariato.

Il ponte copre **tutte** le superfici di agenti/team/workflow/memoria, non solo lo stato:

I nomi qui sotto (`agent.ask`, `team.run`, …) sono gli **identificatori interni del ponte**
(`gmb_state_hook.h`), non comandi BASIC: le superfici del linguaggio sono quelle canoniche — per
esempio `AI.AGENTASKSYNC()` (funzione) / `AI AGENT ASK` (comando), `AI.TEAMLIST()`,
`AI BRAIN LIST` + `AI.BRAINLIST()`. In GMBasic i nomi di superficie **non usano underscore**.

| | superfici (identificatori del ponte) |
|---|---|
| **letture** | `agent.list/get/status/events` · `team.list/agents` · `workflow.list/status/steps` · `memory.list/load/search/ask/rank/consolidate` |
| **scritture** | `agent.create/delete/set/permission/permission_default/toolpolicy/registertool/save/load/trace` · `team.create/delete/add/remove/set_role/set_strategy/set_coordinator/set_recursion/set_latent/set_link/set_thoughts/set_dag/set_complexity/set_reflection/set_critic/set_prune/save/load/message/broadcast/redefine` · `workflow.begin/step/end/execute/delete` · `memory.store/delete` |
| **comportamento** (LLM/loop) | `agent.ask/askfile/askchain/swarm/runcua/rungraph/runrag/runcode/benchmark` · `agent.send/receive/pausa/resume/stop/queuestats/queueclear` · `team.run/run_start/run_wait` · `team.train_thinking/train_ac/distill` |
| **compute/KV** | `agent.cachestats/prefillstats/decodestats/cacheclear/cachetrim/fix` |

Nessuna superficie è "solo managed": i loop `graph.*`/`code.*` sono **implementati in C**
(`gmb_agent_run_graph`/`gmb_agent_run_code` in `native/gmb_agent_runtime`), registrati via seam
dagli host (`gmb_vm_host`, lib BSP) e chiamati **direttamente** dall'emitter C99Native. Nel
processo dell'app lo stato (agenti, provider, brain) vive nel runtime managed e l'hook lo
condivide: è una questione di **stato**, non di capacità.

Il tool set `graph.*` è completo anche in C: `query`, `neighbors`, `path`, `dijkstra`, `bfs`,
`dfs`, `hubs` (grado in+out), `stats` — gli algoritmi sono nel runtime condiviso
(`gmb_graph_brain_query` + op `HUBS` nuova, `gmb_graph_query_multi` per la ricerca) e il loop fa
solo il mapping nome-tool → operazione, con `brain` dal tool o il grafo attivo.

**Errori allineati:** se il runtime managed solleva un'eccezione dentro l'hook, il runtime C la
riceve come testo (`\x01` + messaggio), **non** la inghiotte, e la solleva come errore BASIC: la
pagina si comporta come sulla gamba managed (gestore `ON ERROR` incluso). Prima un
`AI AGENT REGISTER TOOL` fallito era silenzioso (0) sul motore C e stampava il blocco 500 sul
managed — ora è identico e l'errore resta nel log (`[bsp-state] …`).

> Nel frattempo sono stati chiusi due **stub** del VM/C che rendevano impossibili queste
> superfici: `TEAMRUNSTART/TEAMRUNWAIT/TEAMONSTREAM` rispondevano con testi fissi ("Team non
> trovato"/"handle sconosciuto") e `AI.AGENTREGISTERTOOL` ritornava sempre 0 senza registrare
> nulla. Ora delegano al runtime condiviso.

```toml
bspEngine          = "c"     # "c" = VM/C (più veloce) · "managed" = VM P-Code/IL
bspEnginePages     = ""      # pagine da forzare sulla VM/C anche con bspEngine="managed"
bspConcurrentPages = "index.bsp,canvas.bsp,wasm.bsp"   # render C in parallelo (rwlock lato lettura)
```

**Streaming (chunked) e SSE.** Le risposte possono essere inviate **progressivamente**:
`res.StartStream(contentType)` scrive subito status+header con `Transfer-Encoding: chunked`, poi
`res.Write(testo)` + `res.Flush()` mandano i byte mentre l'handler continua (niente
`Content-Length`, `Cache-Control: no-store`, gzip/ETag disattivati: sono semantiche di una
risposta completa). La dashboard usa **`GET /api/events/stream`** (SSE): gli eventi del log
agente arrivano in tempo reale via `EventSource`, con **fallback automatico al polling** di
`/api/events` se il canale cade. Verificato: tick a 400 ms di distanza e un evento generato da
una POST arriva sul canale aperto in <1 s (3 check nel selftest). Ogni connessione resta aperta
~30 s e poi **EventSource riconnette**: un client che sparisce non lascia una coroutine a
girare per minuti (e `SLEEP` è cooperativo: nessun worker occupato).

Lo streaming è implementato su **tutte e quattro le gambe** (IL, PCodeVM, VM/C, C99Native) con lo
stesso comportamento in rete: sulla gamba nativa il protocollo è scritto **una volta sola** in C
(`gmb_network.c`: `gmb_http_response_stream_begin/write/flush/end`), chiamato da VM/C e dal C
emesso da C99Native; sulle gambe managed il framing lo fa il trasporto (socket PCode, SslStream
TLS, `HttpListener`) dietro `CommonLib.Http.HttpStreamSink`. Prova end-to-end:
`shared/minitests/http/stream_chunked.bas` (4 gambe, verifica `Transfer-Encoding: chunked` sul
filo) e l'esempio `shared/samples/Http/HttpStreaming.bas`.

**Concorrenza del motore C.** Il render non è più serializzato in blocco: le pagine in
`bspConcurrentPages` (il cui stato passa **interamente** dall'hook → runtime managed) prendono il
lato **lettura** di un `rwlock` e girano in parallelo; tutte le altre prendono il lato
**scrittura** (serializzate), perché il ramo C toccherebbe registri non protetti. La cattura HTML
è **per-render** (`print_ctx`), i net-hook hanno un mutex dedicato. Misure (`index.bsp`, cache
spenta): c=10 **2.547 → 4.430 rps** (×1,74), c=20 4.674 rps con 0 richieste fallite e risposte
byte-identiche; con la cache attiva (c=10) **52.348 rps**. Analisi:
`Knowledge/BSP-Engine-Audit-Concorrenza`.

**Verifica (obbligatoria dopo aver toccato pagine o superfici AI):**

```sh
bash tools/sh/gmb_c99native_run.sh ../tools/basic/bsp_engine_parity.bas bep -- \
    index.bsp canvas.bsp wasm.bsp
# index.bsp   OK  (46708 byte, identiche)
# canvas.bsp  OK  · wasm.bsp OK
```

Lo strumento imposta i due motori in `.webapp.toml`, riavvia l'app, scarica le pagine,
normalizza i campi dinamici (timestamp/uptime) e confronta; ripristina la configurazione.
Misura del guadagno (PCode, loopback, keep-alive, **cache spenta**): `index.bsp` 163 → **1.436
rps** (c=1) e 179 → **2.340 rps** (c=10). Con `bspCacheMs = 1000` e c=10: `index.bsp` 16,4k rps,
`canvas.bsp` 44,4k, `wasm.bsp` 53,9k, revalidation `304` 68k.

Nota sulle prestazioni: il motore C esegue la pagina in ~0,7 ms; fino al 2026-10-31 ogni run
pagava **+250 ms** di chiusura (il watcher dei timer dormiva fino al cap di 250 ms e il join lo
aspettava) — corretto in `native/gmb_vm_c/gmb_gpm.c` (`gmb_sched_wake_timer_watcher`).

## Configurazione (`.webapp.toml`)

```toml
[server]
dashboardPort = 8090
apiPort       = 8765
allowedIPs    = "127.0.0.1"   # console locale: solo loopback ("" = nessun filtro)
sessions      = true

[app]
seedDemo  = true              # crea Demo/Analyst/Support se il registry è vuoto
autoTrace = true              # AI AGENT TRACE VERBOSE all'avvio (Log popolato)
refreshMs = 2500              # polling delle tab (0 = solo manuale)

[paths]
pages  = "pages"              # relativi alla cartella della app
static = "static"
```

Il file è cercato nella cartella della app e risalendo (max 4 livelli); se manca
valgono i default interni. **Nessun parametro è hardcoded** in `app.bas`.

## Avvio

```sh
# gestione completa (start/stop/restart/status/logs/url) — DEFAULT: compilatore C
tools/sh/webapp_up.sh start            # = start c
tools/sh/webapp_up.sh status
tools/sh/webapp_up.sh logs
tools/sh/webapp_up.sh stop

# alternativa C# (PCode in-process), stesso launcher e stesse porte
tools/sh/webapp_up.sh start csharp

# equivalente manuale (C: build in shared/webapp/bin e avvio)
GMBasic/native/gmb_compiler_c/build/gmb run --project shared/webapp
# equivalente manuale (C#, opzionale)
GMBasicSharp/CompilerCli/bin/Debug/net10.0/gmbcs run --engine pcode shared/webapp/app.bas
```

PID e log stanno in `${TMPDIR}` (`gmb_webapp.pid`, `gmb_webapp.log`), **mai nel repo**.
I percorsi di `pages/` e `static/` sono risolti dalla cartella della app
(`PROJECTFOLDER()`): funziona da qualunque working directory.

| URL | cosa |
|---|---|
| <http://127.0.0.1:8090/index.bsp> | dashboard (8 tab) |
| <http://127.0.0.1:8090/canvas.bsp> · `/wasm.bsp` | canvas e shell WASM stand-alone |
| <http://127.0.0.1:8090/api/status> | stato app/runtime |
| <http://127.0.0.1:8765/api/agents> | API diretta (senza proxy) |

## Verifica end-to-end

```sh
tools/sh/webapp_selftest.sh
```

Avvia la app (se non è già attiva), esercita pagine, statici, CRUD agenti, **persistenza
agenti (file creato/rimosso)**, **team (create/409/aggiungi membro/file/delete)**,
workflow (crea → step → esegui → cancella), memory (store/load/delete), brain, enrich,
handshake WebSocket, poi ripulisce ciò che ha creato e ferma la app. Lo script usa
`webapp_up.sh`, quindi gira sul **runtime C** di default (`start csharp` per il confronto).

### Click-through della UI (browser headless)

I test HTTP non eseguono il JS. `tools/basic/webapp_ui_cdp.bas` apre la dashboard in un browser
headless via CDP e verifica: palette (Ctrl+K + ricerca fuzzy), rail agenti, Inbox, scorciatoie
e tab, tema persistito, **creazione agente dal form**, filtri Log salvati, **SSE live**,
handshake **WebSocket** del canvas, zero errori JS — e lascia uno screenshot in
`/tmp/webapp_ui.png`.

Un dettaglio del browser headless: a volte il **primo** `onopen` del WebSocket non viene
consegnato alla pagina. In quel caso il test verifica l'endpoint **dal contesto della pagina**
con una socket "fresh": se apre, il server WS della webapp è raggiungibile e il controllo
passa con il dettaglio (`UI=closed`, `fresh=open`). Nessun WARN: 20 PASS / 0 WARN / 0 FAIL.

```sh
bash tools/sh/gmb_c99native_run.sh ../tools/basic/webapp_ui_cdp.bas wcdp   # Chromium headless (default)
... -- --browser obscura                                            # Obscura (vedi nota)
... -- --cdp http://127.0.0.1:9222                                 # browser già avviato
```

> **Obscura 0.2.2**: si blocca sul rendering di `index.bsp` (con `<style>` + `<script>` reali
> insieme; svuotando o togliendo uno dei due la pagina si carica, e con Chromium 149 è
> perfetta). È un difetto del motore del browser, non della pagina: tracciato in
> `Bugs/Obscura-Blocco-Index-BSP.md`. Per questo il default dello script è Chromium;
> `--browser obscura` riproduce il blocco.

## Note

- **BSP servite solo se montate**: il branch BSP di `HttpService.Dispatch` lavora sui file
  statici. I mount sono multipli e vince il **prefisso più lungo** (`/` pagine, `/static` asset).
- **Le pagine si compilano al primo hit** (o in anticipo:
  `dotnet …/REPL.dll -noconsole -bsp shared/webapp/pages`). Il sidecar `<pagina>.bsp.chunk`
  è un artefatto: non va committato.
- **SSR + live**: la tabella agenti è renderizzata lato server (funziona senza JS) e poi
  rimpiazzata dal polling. Tutto il testo che arriva dal runtime è **escapato** prima di
  finire nell'HTML: la pagina ha l'escape inline (`& < > " '`), così l'SSR gira anche col
  motore BSP in C (che non esegue le FUNCTION dell'include).
- **Nessun modello cablato**: gli agenti usano il provider attivo (`AI SET ACTIVE …`); senza
  provider la chat risponde con il fallback del runtime (`(insufficient context)`).
- **WASM**: la tab dichiara onestamente se `static/prog.wasm` esiste (404 → istruzioni),
  invece di fingere. Il "Demo draw" verifica il canvas in JS puro.
- **Team**: la creazione/aggiunta passa dall'API perché i comandi BASIC `AI TEAM CREATE`
  / `AI TEAM ADD AGENT` vogliono un nome *statico* nel sorgente; l'API accetta nomi dinamici
  e l'app persiste il file. I team restano comunque entità del runtime (broadcast incluso).
