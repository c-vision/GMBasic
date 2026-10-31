# WEBAPP — Agents Operating Center (BSP + WebSocket + WASM)

Frontend condiviso delle app GMBasic: un **cruscotto per lo stack AI Agents**, scritto
**interamente in BASIC** (nessun C# da toccare). Un solo launcher avvia tutto.

```
shared/webapp/
├── app.bas            launcher: seed agenti, server API, server HTTP, proxy, WebSocket
├── pages/
│   ├── index.bsp      dashboard 5 tab (Agents · Teams · Log · Canvas · WASM) — SSR + JS live
│   ├── canvas.bsp     canvas HTML5 standalone (protocollo WEBCANVAS)
│   └── wasm.bsp       shell del target `.bas → .wasm` (WasmBridge.inc)
└── static/
    ├── canvas.js      replay dei comandi di disegno (CanvasReplay)
    └── wasm.js        glue per il modulo wasm32
```

## Architettura

| componente | porta | cosa fa |
|---|---|---|
| `AI AGENT SERVE` | **8765** | API JSON degli agenti: `/api/agents`, `/api/teams`, `/api/events`, `/api/workflows` (+ `/{n}`, `/{n}/steps`, `/{n}/execute`, `DELETE`), `/api/team/{n}/broadcast`, `/api/agent/{n}/prompt`, `/api/memory` (+ `/search`), `/api/brain/*`, `/api/enrich` |
| `HTTP SERVER` + BSP | **8090** | serve `pages/*.bsp` **renderizzate** e `static/*`; espone tutto `/api/*` come **proxy same-origin** verso 8765 (nessun CORS) |
| `HTTP ROUTE /ws/canvas` | 8090 | WebSocket: spine i comandi di disegno (`WEBCANVAS`) che `static/canvas.js` replaya su `<canvas>` |

Dettagli:

- **Pagine BSP**: HTML + blocchi `<% … %>` eseguiti sul server (SSR) e `<%= … %>` per
  l'interpolazione. Le pagine possono `IMPORT` le librerie BASIC condivise
  (`DASHBOARD`, `WASMBRIDGE`): il pipeline BSP usa la stessa discovery degli `include/`
  del compilatore normale.
- **Proxy**: `RegisterApiRoutes()` registra una route per ogni endpoint e un solo helper
  (`Forward`) inoltra metodo, path e query al server agenti. Aggiungere un endpoint =
  una riga di `HTTP ROUTE` + l'implementazione lato agent server.
- **Canvas**: `WEBCANVAS.*` (in `shared/include/WebCanvas.inc`) serializza i comandi in JSON
  e li invia sul WebSocket; il browser li ridisegna. Nessuno stato globale: ogni SUB riceve
  il `ws`.
- **WASM**: `wasm.bsp` + `static/wasm.js` sono la shell del target `.bas → .wasm`
  (gli import `DECLARE LIB "env"` sono in `shared/include/WasmBridge.inc`).

## Avvio

Dalla **root del monorepo**:

```sh
# backend di default del REPL (VM/C): richiede gmb_vm_host compilato
dotnet GMBasicSharp/REPL/bin/Debug/net10.0/REPL.dll -noconsole shared/webapp/app.bas

# oppure con il runner PCode (nessun processo esterno)
tools/sh/harness.sh pcode shared/webapp/app.bas
```

Poi:

| URL | cosa |
|---|---|
| <http://localhost:8090/index.bsp> | dashboard (5 tab) |
| <http://localhost:8090/canvas.bsp> | canvas standalone |
| <http://localhost:8090/wasm.bsp> | shell WASM |
| <http://localhost:8090/api/agents> | API via proxy same-origin |
| <http://localhost:8765/api/agents> | API diretta |

I percorsi di `pages/` e `static/` sono risolti dalla **cartella della app**
(`PROJECTFOLDER()`), quindi l'app funziona da qualunque working directory.

## Verifica rapida (end-to-end)

```sh
tools/sh/harness.sh pcode shared/webapp/app.bas &   # oppure il REPL
curl -s http://127.0.0.1:8090/index.bsp | head -5            # SSR: tabella agenti
curl -s http://127.0.0.1:8090/static/canvas.js -o /dev/null -w '%{http_code}\n'
curl -s http://127.0.0.1:8090/api/agents
curl -s -X POST -H 'Content-Type: application/json' -d '{"name":"Smoke"}' \
     http://127.0.0.1:8090/api/workflows
curl -s -X POST -H 'Content-Type: application/json' -d '{"name":"s1","action":"PRINT 1"}' \
     http://127.0.0.1:8090/api/workflows/Smoke/steps
curl -s -X POST -d '{}' http://127.0.0.1:8090/api/workflows/Smoke/execute
```

WebSocket (handshake + primi frame del canvas):

```sh
curl -s -i -N -m 5 -H 'Connection: Upgrade' -H 'Upgrade: websocket' \
     -H 'Sec-WebSocket-Version: 13' -H 'Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==' \
     http://127.0.0.1:8090/ws/canvas
```

## Note

- **BSP servite solo se il file è nella cartella montata con `HTTP STATIC`**: il branch BSP
  di `HttpService.Dispatch` lavora sui file statici. I mount sono multipli e vince il
  **prefisso più lungo** (`/` per le pagine, `/static` per gli asset).
- **Le pagine sono compilate al primo hit** (e in anticipo con
  `dotnet …/REPL.dll -noconsole -bsp shared/webapp/pages`). Il sidecar `<pagina>.bsp.chunk`
  è generato accanto al sorgente: è un artefatto, non va committato.
- I modelli LLM **non** sono cablati nel codice: gli agenti usano il provider attivo
  (`AI SET ACTIVE …`); senza provider la chat risponde con il fallback dell'agent server.
