# WEBAPP — Agents Operating Center

Cruscotto web dello stack **AI Agents** di GMBasic, scritto **interamente in BASIC**:
un launcher, cinque pagine BSP e due file JS. Nessun C# da toccare, nessun build step.

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
| `AI AGENT SERVE` | **8765** | API JSON del runtime: `/api/agents`, `/api/teams`, `/api/events`, `/api/workflows` (+ `/{n}`, `/{n}/steps`, `/{n}/execute`, `DELETE`), `/api/team/{n}/broadcast`, `/api/agent/{n}/prompt`, `/api/memory` (+ `/search`), `/api/brain/*`, `/api/enrich` |
| `HTTP SERVER` + BSP | **8090** | serve `pages/*.bsp` renderizzate e `static/*`; espone **tutta** `/api/*` come proxy same-origin verso 8765 (niente CORS); endpoint propri della app |
| `HTTP ROUTE /ws/canvas` | 8090 | WebSocket: `WEBCANVAS` spinge frame JSON che `canvas.js` disegna |

### Endpoint propri della app (implementati in `app.bas`)

| Metodo | Path | Cosa fa |
|---|---|---|
| GET | `/api/health` | liveness (`{"ok":true}`) |
| GET | `/api/status` | stato app+runtime: provider attivo, modello, agenti, eventi, uptime, porte, reachability dell'API, contatore proxy |
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
# gestione completa (start/stop/restart/status/logs/url) — backend PCode in-process
tools/sh/webapp_up.sh start
tools/sh/webapp_up.sh status
tools/sh/webapp_up.sh logs
tools/sh/webapp_up.sh stop

# equivalente manuale
dotnet GMBasicSharp/REPL/bin/Debug/net10.0/REPL.dll -noconsole --pcode shared/webapp/app.bas
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
handshake WebSocket, poi ripulisce ciò che ha creato e ferma la app. Oggi: **43 PASS / 0 FAIL**
(identico su PCode e IL).

## Note

- **BSP servite solo se montate**: il branch BSP di `HttpService.Dispatch` lavora sui file
  statici. I mount sono multipli e vince il **prefisso più lungo** (`/` pagine, `/static` asset).
- **Le pagine si compilano al primo hit** (o in anticipo:
  `dotnet …/REPL.dll -noconsole -bsp shared/webapp/pages`). Il sidecar `<pagina>.bsp.chunk`
  è un artefatto: non va committato.
- **SSR + live**: la tabella agenti è renderizzata lato server (funziona senza JS) e poi
  rimpiazzata dal polling. Tutto il testo che arriva dal runtime è **escapato**
  (`DASHBOARD.EscapeHtml`) prima di finire nell'HTML.
- **Nessun modello cablato**: gli agenti usano il provider attivo (`AI SET ACTIVE …`); senza
  provider la chat risponde con il fallback del runtime (`(insufficient context)`).
- **WASM**: la tab dichiara onestamente se `static/prog.wasm` esiste (404 → istruzioni),
  invece di fingere. Il "Demo draw" verifica il canvas in JS puro.
- **Team**: la creazione/aggiunta passa dall'API perché i comandi BASIC `AI TEAM CREATE`
  / `AI TEAM ADD AGENT` vogliono un nome *statico* nel sorgente; l'API accetta nomi dinamici
  e l'app persiste il file. I team restano comunque entità del runtime (broadcast incluso).
