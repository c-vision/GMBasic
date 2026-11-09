# V.36 — Computer use e browser

## Cosa ottieni da questo capitolo

Far agire un programma su un'interfaccia che non è la sua: un browser, una finestra, il desktop.
È la capacità più potente e la più rischiosa della Parte V, perché l'azione ha effetti reali su
sistemi reali.

## 1. Browser: il ponte

L'automazione del browser in GMBasic è un **runtime di azioni**: naviga, legge il DOM, clicca,
digita, aspetta una condizione, estrae. `Browser-Automation-Guida-Completa` è il capitolo di
riferimento; i sample `Browser/BrowserC1C4.bas` … `BrowserE14.bas` sono una progressione di casi.

Il pattern normale non è "clicca su queste coordinate", è:

```
naviga → attendi la condizione → leggi lo stato → agisci → verifica
```

L'attesa di una **condizione** (un selettore presente, un testo visibile) è ciò che distingue
un'automazione stabile da una che fallisce a caso perché la pagina era lenta.

## 2. Il modello di azione

| Azione | Nota |
|---|---|
| navigare | URL, cronologia, ricarica |
| leggere | testo, attributi, elenco di elementi |
| cliccare | per selettore, non per coordinate |
| digitare | con invio opzionale |
| aspettare | condizione, non tempo fisso |
| valutare | espressione nella pagina (potente, da usare con criterio) |

`Browser-Automation-Plugin` descrive l'integrazione come plugin: il browser diventa uno
**strumento** dell'agente, con i suoi permessi.

## 3. Agente + browser

`Ai/BrowserAgentTools.bas` e `Browser/AiAgentBrowserToolLoop.bas` mostrano il ciclo completo:
l'agente decide, lo strumento esegue, il risultato torna come osservazione. È qui che l'agente
smette di essere un chatbot e diventa qualcosa che **fa**.

I rischi, in ordine di gravità:

1. **azioni distruttive** — un click su "elimina" non si annulla;
2. **sessioni autenticate** — il browser può essere già loggato;
3. **dati esfiltrati** — un agente può copiare contenuti in un form;
4. **prompt injection** — il testo di una pagina può contenere istruzioni.

La quarta è la più insidiosa: il contenuto di una pagina web è **dato**, non un ordine. Un agente
che esegue istruzioni trovate in una pagina è un agente compromesso.

## 4. Computer use

`cua` (Computer-Universal-Action) estende le azioni oltre il browser: schermo, tastiera, mouse,
finestre native. I sample `Computer/ComputerType.bas` e `Computer/ComputerScreenRecord.bas`
mostrano digitazione e registrazione dello schermo; `CUA-Universal-Action-Runtime-Guida-Utente`
descrive il runtime.

Il computer use è l'ultima risorsa: è più lento, più fragile e più pericoloso di un'API. Se esiste
un'API per fare la stessa cosa, usa l'API.

## 5. Testare un'automazione

Un'automazione si testa come tutto il resto (III.15), con due difficoltà in più:

- **l'ambiente cambia**: la pagina non è la stessa, il selettore si rompe;
- **il tempo conta**: un test che aspetta 3 secondi fissi è lento e instabile.

La soluzione è la stessa in entrambi i casi: condizioni esplicite al posto di attese fisse, e
fixture locali (una pagina statica) invece di siti esterni.

## 6. Perché non c'è un output

Questi sample richiedono un browser, un desktop e spesso una sessione autenticata. In questo
ambiente non ci sono, e un'automazione "dimostrata" su una pagina finta non proverebbe nulla sul
mondo reale. Il capitolo cita il codice e i sample; non simula un risultato.

## 7. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| il click fallisce a caso | attesa fissa invece di condizione | attendi il selettore |
| il selettore non esiste più | pagina cambiata | usa selettori semantici (`role`, testo) |
| il test passa a mano e non in CI | dipende da una sessione | fixture locale, stato pulito |
| l'agente esegue ordini trovati nella pagina | prompt injection | tratta il contenuto come dato |
| azione irreversibile | nessuna conferma | richiedi approvazione per le azioni distruttive |

## 8. Dove andare adesso

- **V.33** — l'agente che usa questi strumenti, e i suoi permessi.
- **IV.24** — l'alternativa: un'API HTTP invece di un browser.
- **III.15** — testare un'automazione in modo deterministico.













<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### Browser-Automation-Guida-Completa

*Fonte: `Browser-Automation-Guida-Completa`*


## 1. Architettura in una riga

BASIC (shared/samples/Browser/*.bas)  →  plugins/browser/browser.bas (procedure CDP)
        │                                    │
        └─ AI AGENT / tool-loop  ◄── McpToolRegistry (tool.json) ◄── PluginIndex (trigger)


## 2. Cosa può fare un agente (capacità finali)

### Navigazione e lettura

### Interazione

### Rete, sessione, sicurezza

BROWSER.SETUSERAGENT("Mozilla/5.0 ...")   ' imposta lo user-agent
BROWSER.HUMANFILL("@eN", "testo")         ' compila simulando digitazione umana
cookies = BROWSER.GETCOOKIES()            ' legge i cookie della sessione
BROWSER.CLEARCOOKIES()                    ' azzera i cookie


### AI / visione

### Sessione, memoria, test

## 3. I builtin AI aggiunti

### `AI.ANALYZEIMAGE(path, prompt)` — visione

### La GPM resta l'unico esecutore

## 4. Acquisizione automatica della skill (per-ask, contesto minimo)

"naviga su example.com"  → 38 tool browser nel set di QUEL turno (naviga + legge il testo)
"fai un riassunto"       → 0 tool browser (contesto di default invariato)


## 5. Le fasi implementate (piano AI-Agent-Automation)
| Fase | Capacità | Verifica |
|---|---|---|
| **C1** | Selezione pagina/tab dal `/json` (indice/URL/titolo) | IL + PCode |
| **C2** | Timeout comandi CDP configurabile | IL + PCode |
| **C3** | Attesa per condizione (selettore/testo in polling) | IL + PCode |
| **C4** | Click / fill / scroll via `Runtime.evaluate` | IL + PCode |
| **C5** | Errori CDP espliciti (protocollo, `exceptionDetails`, timeout) | IL + PCode |
| **C6** | Download file (`Browser.setDownloadBehavior`) | IL + PCode |
| **D1** | Loop computer-use: screenshot + `AI.ANALYZEIMAGE` → decisione → azione | IL + PCode |
| **D2** | Sessione multi-step: back/forward, tab, stato, memoria agente | IL + PCode |
| **D3** | Rete/auth: cookie, UA, header (Fetch interception), proxy, tool di default | IL + PCode |
| **D4** | PDF → estrazione (`CONVERT.ToText`/`gmb_doc_pdf.c`) → ingestion RAG | IL + PCode |
| **E2** | Sessione persistente: profilo stabile + cookie con expiry (login riusato) | IL + PCode |
| **E3** | Web→RAG diretta: naviga, chunk, indicizza nel vector DB | IL + PCode |
| **E8** | Visual QA: screenshot + vision → asserzione visiva | PCode |
| **E9** | Human-like: mouse a curve, typing naturale, scroll incrementale | PCode |
| **E10** | Profilo coerente + anti-fingerprint (`sec-ch-ua`, viewport, `webdriver=false`) | PCode |
| **E11** | Warm start: processo Chromium riusato tra run | — |
| **E12** | Osservabilità: log con prove visive (screenshot a ogni step), export | PCode |
| **E13** | Intercettazione richieste (`Network.setBlockedURLs`) | PCode |
| **E14** | Memoria del piano: stato su file, riprende dopo riavvio | PCode (save ok; read flaky WIP) |
| **E15** | Sandbox: `--no-sandbox` condizionale, `--disable-extensions` | — |
| **E16** | Etica/cortesia: robots.txt, rate-limit, avviso CAPTCHA | PCode |
| **E17** | Deterministic replay per test E2E | PCode |

## 6. Lezione / gotcha emersi (per chi estende)

## 7. Comandi utili

# setup dei due browser (manifest in Resources/, cache in ~/.gmbasic/libs/)
gmb setup BROWSER          # Chromium (Chrome for Testing, ~300MB)
gmb setup OBSCURA          # Obscura (Rust/V8, ~70MB, per agenti)
gmb setup status           # stato cache di BROWSER + OBSCURA
gmb setup rm OBSCURA --force

# avviare il browser (profilo stabile; riusa se già attivo = warm start)
plugins/browser/launch_chromium.sh 9222          # GMBASIC_CHROMIUM_EPHEMERAL=1 per test isolati
# Obscura: obscura --remote-debugging-port=9222 (CDP, connectOverCDP)
pkill -f "remote-debugging-port=9222"

# eseguire un sample
GMBASIC_HEADLESS=1 GMBASIC_V2_TIMEOUT_MS=30000 dotnet run --project test_ilrun/test_ilrun.csproj \
  -- shared/samples/Browser/BrowserD1.bas          # loop computer-use
# --engine=pcode  per i flussi multi-WS (E8-E10, E13, E16, E17)



## 8. File chiave

## 9. Browser per sessione agente — da BASIC (2026-09-06)

AI AGENT CREATE "r", "Sei un ricercatore."
AI AGENT SET "r" BROWSER %OBSCURA   ' chromium|obscura|auto (senza virgole)
PRINT AI.AGENTGET("r", "browser")     ' → obscura
PRINT AI.AGENTSTATUS("r")             ' → ... browser=obscura ...
' auto = obscura se cached (gmb setup OBSCURA) altrimenti chromium
AI AGENT SET "r" BROWSER %AUTO


## 10. Funzioni avanzate Camoufox — via CDP puro, anti-dipendenza (2026-09-08)

### 10.1 Anti-detection / fingerprint spoofing (stealth)

### 10.2 Snapshot accessibility compatto con ref stabili — `BROWSER.ACCESSTREE()`

PRINT BROWSER.ACCESSTREE()
' @e1 RootWebArea
' @e5 button "OK"
' @e6 StaticText "OK"


### 10.3 Azione sui ref — `BROWSER.CLICK("@eN")` / `BROWSER.FILL("@eN", ...)`

### 10.4 Isolamento sessione per profilo — `BROWSER.START(engine, port, profile)`

PRINT BROWSER.START(%CHROMIUM, 9255, "demo-userX")
' → ~/.gmbasic/libs/chromium/profiles/demo-userX


### 10.5 Copertura backend (onesto)
| Funzione | IL | PCodeVM | VM/C | C99Native |
|---|---|---|---|---|
| `BROWSER.ACCESSTREE()` | ✅ | ✅ | ✅ | ✅ |
| stealth / ref-click `@eN` / profilo | ✅ | ✅ | ❌ | ❌ |

## 11. Agent Governance — nuove funzioni (2026-09-13, `Todo/(solved) - Browser-Agent-Governance`)

### 11.1 G1 — `BROWSER.OBSERVE([preset$])`

DIM obs$ = BROWSER.OBSERVE()
PRINT obs$
' {"url":..., "title":..., "interactables":[{id,role,name,ref,bbox}], "consoleErrors":[], "tabs":[...]}


### 11.2 G2 — verifica before/after (niente "clicked?" cieco)

### 11.3 G3 — `BROWSER.FIND(testo$[, regex])`

DIM ris$ = BROWSER.FIND("Click Me")      ' [{"tag":"BUTTON","text":"Click Me"}]
DIM ris2$ = BROWSER.FIND("^(Login|Sign)", TRUE)   ' regex


### 11.4 G5 — `BROWSER.NETLOG([clear])` e `BROWSER.CONSOLE([clear])`

BROWSER.NAVIGATE("...")
DIM net$ = BROWSER.NETLOG(TRUE)     ' richieste/risposte bufferizzate
DIM con$ = BROWSER.CONSOLE(TRUE)    ' console.log/warn/error


### 11.5 G6 — `BROWSER.AUTH(op$, nome$)` — profili di autenticazione

BROWSER.AUTH("save", "gmail")        ' salva i cookies correnti
BROWSER.AUTH("list", "")             ' elenca i profili
BROWSER.AUTH("load", "gmail")        ' ripristina i cookies
BROWSER.AUTH("fork", "ramo,default") ' G7: clona stato in nuova sessione CDP


### 11.6 G4 — governance profile `%FAST` / `%GOVERNED` (risk_category)

AI AGENT SET "r" PROFILE %FAST      ' default — behavioral odierno
AI AGENT SET "r" PROFILE %GOVERNED  ' strict — ogni scrittura approvata


### 11.7 G10 — audit hash-chain (SHA256)

### 11.8 G11 — MCP resources `browser://` vive

### 11.9 Sample

### 11.10 Matrice parità backend (onesta)
| Funzione | IL | PCodeVM | VM/C | C99Native |
|---|---|---|---|---|
| `BROWSER.OBSERVE` | ✅ | ✅ | ✅ | ✅ |
| `BROWSER.FIND` | ✅ | ✅ | ✅ | ✅ |
| `BROWSER.NETLOG`/`CONSOLE` | ✅ | ✅ | ✅ | ✅ |
| `BROWSER.AUTH` list (save/load/fork → errore CDP) | ✅ | ✅ | ✅ | ✅ |
| G4 `PROFILE %FAST/%GOVERNED` | ✅ | ✅ | ❌ | ❌ |
| G10 hash-chain audit | ✅ | ✅ | ❌ | ❌ |
| G11 `browser://` MCP | ✅ | ✅ | ❌ | ❌ |
| Funzione | Firma | Note |
|---|---|---|
| `BROWSER.HUMANCLICK(sel$)` | click con movimento umano del mouse | richiede CDP |
| `BROWSER.HUMANSCROLL(dy)` | scroll umano di `dy` pixel | richiede CDP |
| `BROWSER.SETCOOKIE(name$, value$[, domain$])` | imposta un cookie | richiede CDP |
| `BROWSER.SETHEADER(name$, value$)` | header HTTP extra | richiede CDP |

### Browser-Automation-Plugin

*Fonte: `Browser-Automation-Plugin`*


## 1. Cos'è

## 2. Funzioni del plugin
| Fase | Funzione | Comportamento |
|---|---|---|
| base | `BrowserNavigate(url, port)` | apre una pagina (`Page.navigate`) |
| base | `BrowserTitle(port)` / `BrowserText(port)` | titolo / testo visibile |
| base | `BrowserEvaluate(js, port)` | esegue JS (returnByValue) |
| base | `BrowserScreenshot(out, port)` | cattura PNG → JSON raw CDP (il salvataggio lo fa `BrowserAnalyze`) |
| base | `BrowserWait(port)` | attesa ms fissi (legacy) |
| **C1** | `BrowserConnect(port, pageIndexOrUrl)` | sceglie la pagina/tab dal `/json`: vuoto=prima, `"0","1",..`=per indice, altrimenti frammento di **URL o titolo**. Fallback a `CdpPath` |
| **C2** | `BrowserSetTimeout(port, commandTimeoutMs)` | timeout dei comandi CDP (default 10s) |
| **C3** | `BrowserWaitFor(port, selectorOrText, timeoutMs)` | polling `Runtime.evaluate` finché il selettore esiste o il testo appare → `"found"` o `""` |
| **C4** | `BrowserClick(port, selector)` | click su selettore CSS |
| **C4** | `BrowserFill(port, selector, value)` | compila input (con evento `input`) |
| **C4** | `BrowserScroll(port, dx, dy)` | `window.scrollBy` → JSON CDP con la nuova `scrollY` |
| **C5** | `BrowserCdpError(raw)` | estrae l'errore da un risultato CDP: `"error"` del protocollo (`message`) o `exceptionDetails` di `Runtime.evaluate` (`description`) |
| **C6** | `BrowserDownload(port, url, destDir)` | `Browser.setDownloadBehavior` (allow + `downloadPath`) + navigate → il file atterra in `destDir` |
| **D1** | `BrowserAnalyze(port, prompt, outPath)` | screenshot (estrae `data` base64 + `FROMBASE64` + `PUT` in `outPath`) + `AI.ANALYZEIMAGE(outPath, prompt)` → decisione (es. `"click #btn"`) |
| **D2** | `BrowserBack(port)` / `BrowserForward(port)` | `history.back()`/`forward()` via JS (`Page.goBack` rimosso da Chrome 149) |
| **D2** | `BrowserState(port)` | snapshot JSON `{u,t,l}` (url / titolo / history.length) |
| **D2** | `BrowserTabs(port)` | elenca le pagine dal `/json` (`indice|url|titolo`) |
| **D2** | `BrowserSessionRecord(port, action, detail)` / `BrowserSessionLog(port)` | memoria della sessione (passi fatto/visto) |
| **D4** | `BrowserDownloadPdf(port, url, destDir, fileName)` | scarica un PDF da un endpoint base64 (FETCH; Chrome 149 apre i PDF inline, non li scarica) + `CONVERT.ToText` (gmb_doc_pdf.c) → testo per RAG |
| **E2** | `BrowserSetCookiePersistent(name, value, url, port)` | cookie con `expires` (2036) nel profilo stabile → sopravvive al riavvio (login una volta) |
| **E3** | `BrowserToRag(url, db, coll, docId, pattern, port)` | naviga la pagina live, chunk per pattern (SPLIT), indicizza ogni chunk nel vector DB (id docId-N) → RAG senza file intermedi |
| **E11** | `BrowserPing(port)` | warm start: verifica che il CDP sia già attivo (processo riusato) |
| **E8** | `BrowserAssert(port, assertion, outPath)` | screenshot + vision → verdetto `true`/`false` su uno stato visivo (Visual QA) |
| **E9** | `HumanClick`/`HumanType`/`HumanScroll`/`HumanDelay` | azioni human-like: mouse a curve, typing char-by-char, scroll incrementale, delay non uniformi |
| **E10** | `BrowserProfile(port, ua, width, height)` | profilo coerente + anti-fingerprint: UA+sec-ch-ua, viewport, `navigator.webdriver` rimosso |
| **E11** | `BrowserPing(port)` | warm start: verifica che il CDP sia già attivo (processo riusato) |
| **E12** | `BrowserAuditStep`/`BrowserAuditExport` | osservabilità: ogni step nel log CON screenshot (prova visiva), export in audit.log |
| **E13** | `BrowserBlockUrls(port, patternsJson)` | intercettazione richieste: `Network.setBlockedURLs` (ads/analytics) |
| **E14** | `BrowserTaskSave`/`BrowserTaskResume` | memoria del piano: stato (URL+log) su file, riprende dopo riavvio |
| **E16** | `BrowserRobotsAllowed`/`BrowserRateLimit`/`BrowserCheckCaptcha` | etica/cortesia: robots.txt, rate-limit, avviso CAPTCHA |
| **E17** | `BrowserReplay(port, log)` | deterministic replay: riproduce le azioni registrate (test E2E) |

## 3. `AI.ANALYZEIMAGE(path, prompt)` (D1)

## 4. Gotcha emersi (compilando il plugin per intero)

## 5. PROCESS e BrowserExtract (fondamento E0/C7 — 2026-08-31)

### `AI PROCESS file PROMPT istruzioni → stringa` (builtin 1227, IL/PCode)

' Testo estratto (pdf/docx/xlsx/csv/md/html/rtf/epub/odt/pptx via ConversionLib, json/testo raw) → agente → risposta
risultato$ = AI.PROCESS("work/report.pdf", "Estrai le revenue per divisione e indicizza nel RAG 'report'")
PRINT risultato$

' Esempio reale (sample ProcessDoc.bas)
AI PROCESS "shared/samples/Fixtures/process-report.txt" PROMPT "Riassumi in 3 punti."


### `AI.BROWSEREXTRACT(url, prompt, schema) → stringa JSON` (plugin, IL/PCode)

IMPORT AI

j$ = AI.BROWSEREXTRACT("https://example.com", "estrai il titolo", "{""title"":""string""}")
PRINT j$   ' → {"title":"Example Domain"}


### Setup Chromium / Obscura (dual provider, CUA §10)

' REPL o CLI: scarica il browser con SHA256 verificato
' gmb setup BROWSER   (CLI)   o   SETUP BROWSER   (REPL)   → Chromium (Chrome for Testing ~300MB)
' gmb setup OBSCURA   (CLI)   o   SETUP OBSCURA   (REPL)   → Obscura (Rust+V8 ~70MB, per agenti)
' → ~/.gmbasic/libs/{chromium,obscura}/<rid>/ + marker .gmb-{chromium,obscura}-ok
'     (Resources/{chromium,obscura}-manifest.json, GmbiPaths.ChromiumDir/ObscuraDir)
' gmb setup status    → stato cache di BROWSER + OBSCURA


AI AGENT CREATE "r", "Sei un ricercatore."
AI AGENT SET "r" BROWSER %OBSCURA   ' chromium|obscura|auto (senza virgole)
PRINT AI.AGENTGET("r", "browser")     ' → obscura
PRINT AI.AGENTSTATUS("r")             ' → ... browser=obscura ...
' auto → obscura se cached (gmb setup OBSCURA) altrimenti chromium


## 6. Sample + verifica
| Sample | Fasi | Backend verificato |
|---|---|---|
| `BrowserC1C4.bas` | C1-C4 (self-contained con `data:` URL) | IL ✓, PCode ✓ |
| `BrowserC5C6.bas` | C5-C6 (errore JS/protocollo + download) | da completare (interazione IL flaky sul call di `BrowserCdpError`, in indagine) |
| `BrowserD1.bas` | D1 (screenshot→vision→click) | IL ✓, PCode ✓ |
| `BrowserD2.bas` | D2 (navigate→back→forward→tabs→session log) | IL ✓, PCode ✓ |
| `BrowserD4.bas` | D4 (download PDF→estrazione→vector→search) | IL ✓, PCode ✓ |
| `BrowserE2Persist.bas` + `BrowserE2Check.bas` | E2 (cookie persistente tra riavvii) | IL ✓, PCode ✓ |
| `BrowserE3.bas` + `BrowserE3Query.bas` | E3 (web→RAG su file DB, query cross-programma) | IL ✓, PCode ✓ |
| `BrowserE8.bas` | E8 (Visual QA: assert true/false) | PCode ✓ (IL: race WS nota sul 2° connect) |
| `BrowserE9.bas` | E9 (human click+type+scroll → `done:gm`) | PCode ✓ |
| `BrowserE10.bas` | E10 (profilo coerente, viewport, anti-webdriver) | PCode ✓ |
| `BrowserPluginTest.bas` | B3 base (navigate/text/evaluate/screenshot) | IL ✓ |
| `BrowserAgentTools.bas` | D3 (tool di default senza REGISTER TOOL) | IL ✓ |
| `AiAgentBrowserToolLoop.bas` | B5 (round-trip agente→tool) | IL ✓ |
| `ProcessDoc.bas` | E0 PROCESS | IL ✓, PCode ✓ |
| `BrowserExtractTest.bas` | C7 BrowserExtract | IL ✓, PCode ✓ |

bash plugins/browser/launch_chromium.sh 9222 &
GMBASIC_HEADLESS=1 GMBASIC_V2_TIMEOUT_MS=30000 dotnet run --project test_ilrun/test_ilrun.csproj -- shared/samples/Browser/BrowserD1.bas
pkill -f "remote-debugging-port=9222"; rm -rf /tmp/gmb-chromium.*


## 7. Nota architetturale (comando ≠ funzione)

## 8. Riferimenti

### CUA-Universal-Action-Runtime-Guida

*Fonte: `CUA-Universal-Action-Runtime-Guida`*


## 1. Cos'è il CUA in GMBasic

### 1.1 I tre livelli (L2/L1/L0)
| Livello | Cosa | Funzioni BASIC | Note |
|---|---|---|---|
| **L2 — Tool nativi** | operazioni già integrate (file/db/pdf/http) | `FILES.*`, `DBF.*`, `PDF.*`, `HTTP.*` | sempre preferiti: non servono pixel |
| **L1 — UI semantica** | trova/preme campi e bottoni per testo | `UI.FIND/CLICK/TYPE` | via AX (macOS) / UIA (win) / AT-SPI (linux) |
| **L0 — Computer use** | fallback universale a coordinate/pixel | `COMPUTER.SCREENSHOT/CLICK/DOUBLECLICK/TYPE/KEY/HOTKEY/WINDOWS` | screenshot, mouse, tastiera (12 primitive CUA #7: 12/12 dal 2026-09-13) |

### 1.2 Regola architetturale inderogabile

## 2. Installazione e prerequisiti

## 3. Browser

### 3.1 Avvio e navigazione

PRINT BROWSER.START(%AUTO, 9222)      ' lancia Obscura/Chromium se non attivo e connette
PRINT BROWSER.NAVIGATE("https://example.com")
PRINT BROWSER.EVALUATE("document.title")
PRINT LEFT$(BROWSER.SNAPSHOT(), 60)
BROWSER.CLICK("a.login")
BROWSER.FILL("#user", "mario")


### 3.2 Porta CDP

### 3.3 Funzioni avanzate Camoufox (CDP puro, anti-dipendenza — 2026-09-08)

PRINT BROWSER.ACCESSTREE()
' @e5 button "OK"   (@eN = ref stabile, riusabile tra interazioni)
BROWSER.CLICK("@e5")                 ' azione per ref (client CDP managed, IL/PCode)
BROWSER.FILL("@e4", "hello")


## 4. Computer use (L0)

PRINT LEFT$(COMPUTER.WINDOWS(), 120)       ' finestre visibili (JSON)
PRINT COMPUTER.SCREENSHOT("work/shot.png")  ' PNG reale
COMPUTER.MOVE(320, 320)
COMPUTER.CLICK(320, 320)
COMPUTER.DOUBLECLICK(320, 320)             ' doppio clic (2 eventi a 120 ms)
COMPUTER.TYPE("ciao")
COMPUTER.KEY("Return")
COMPUTER.HOTKEY("ctrl+alt+shift+z")        ' combo: cmd|ctrl|alt|opt|shift + tasto; -1 se malformata


r = COMPUTER.HOTKEY("ctrl+alt+shift+j")   ' 0 se Accessibilità concessa
r = COMPUTER.HOTKEY("hyper+z")            ' -1 (modificatore ignoto, nessun evento)


### 4.1 Screen recording — `SCREENRECORD*`

DIM start$  = COMPUTER.SCREENRECORDSTART("work/rec_test.mp4", 10)
' ... lavoro dell'agente (click/type) ...
DIM stop$   = COMPUTER.SCREENRECORDSTOP()
DIM status$ = COMPUTER.SCREENRECORDSTATUS()


## 4-bis. AI OBSERVE (P1 AI-Runtime: osservazioni grounded)

AI OBSERVE FILE "shared/samples/Fixtures/ai/fattura_it.txt" INTO id
AI OBSERVE LOG "work/run.log", 200 INTO idl
n = AI.OBSERVATIONCOUNT()          ' quante osservazioni nel registry
h$ = AI.OBSERVATIONHASH$(id)       ' FNV-1a64 hex del contenuto (id numerico!)
l$ = AI.OBSERVATIONLIST$()         ' righe id|type|hash
p$ = AI.OBSERVATIONFILE$(id)       ' path dell'artefatto registrato (png/log/dump)
u$ = AI.OBSERVATIONLATEST$()       ' id dell'ultima osservazione ("" se nessuna)
o$ = AI.OBSERVATION$(id)           ' JSON del ContextObject (id/type/source/artifact/hash/size)

| fonte | comando | esito |
|---|---|---|
| DATABASE | `AI OBSERVE DATABASE "<path.sqlite>"[, "<query>"] INTO id` | ✅ 4 backend: registra `type=database` + artefatto `work/ai_observe_database.json` (path/query/rows) |
| PROCESS | `AI OBSERVE PROCESS "<nome>" INTO id` | ✅ 4 backend: registra la query se il processo esiste; se non esiste → **errore esplicito** (catturabile con `ON ERROR`) |
| TEST | `AI OBSERVE TEST "<path.bas>" INTO id` | ✅ 4 backend: compila con lo stesso strumento di `AI VERIFY … COMPILE` e registra l'esito |
| BROWSER | `AI OBSERVE BROWSER "<url>" INTO id` | ✅ 4 backend per il percorso d'errore (URL/ambiente senza browser → errore esplicito, immediato); il percorso di *successo* richiede un browser con CDP attivo |

## 4-ter. AI CONTEXT (P2: Context Engine — policy EFFETTIVA)

' Comandi (spazio, §2.0)
AI CONTEXT SET BUDGET "worker1", 16000                  ' budget in caratteri
AI CONTEXT SET POLICY "worker1", "truncate"             ' compact (default)|truncate|error
AI CONTEXT SET SELECTOR "worker1", "task,deps,freshness" ' conservato (v1: non applicato)
AI CONTEXT VIEW "worker1", "plan,notes,solution: text"  ' vista curata

' Funzioni di lettura (parentesi, §2.1)
n  = AI.CONTEXTUSED("worker1")                ' caratteri della vista REALE (già normalizzata)
b  = AI.CONTEXTBUDGET("worker1")              ' budget configurato
o  = AI.CONTEXTOVERFLOW("worker1")            ' caratteri del testo ORIGINALE oltre budget
v$ = AI.CONTEXTVIEW$("worker1")               ' testo della vista curata
d$ = AI.CONTEXTVIEWDEBUG$("worker1")          ' v1 alias di VIEW


## 4-quater. AI WORKER + AI STATE (P3: INVOCAZIONE PULITA + workspace)

' Comandi (spazio, §2.0)
AI WORKER NEW "solver", {"workspace":"work/solver","budget":32000}  ' crea worker + workspace, lo rende CORRENTE
AI WORKER "ANALYZE"                 ' INVOCAZIONE PULITA sul worker corrente (task/fase)
AI WORKER RUN "solver"              ' round di lifecycle sul worker indicato (solo contatori)
AI WORKER LOOP "IMPLEMENT", 3       ' LOOP ISOLATO: 3 round, budget CUMULATIVO del run, best-so-far
AI STATE SAVE "solver", "notes", "greedy algorithm"   ' memoria esterna del run
AI STATE LOAD "solver", "notes"                       ' lettura (funzione: AI.STATELOAD)

' Funzioni di lettura (parentesi, §2.1)
n  = AI.WORKERROUNDS()        ' round tentati (invocazioni)
r  = AI.WORKERRESCUED()       ' invocazioni fallite/annotate (rescue)
s  = AI.WORKERSPEND()         ' caratteri di vista consumati (budget test-time)

| esito | significato | tracce |
|---|---|---|
| `1` | invocazione eseguita | `analysis`/`solution` salvata, `rounds++`, `spend += chars` |
| `0` | **rifiutata dal budget** (policy `error`) | `rescued++`, `state/last_error = "context overflow: N > budget M"` |
| `-1` | nessun worker corrente | — |
| `-2` | errore provider | `rescued++`, `state/last_error = "Error: nessun provider LLM configurato (AI SET ACTIVE)"` |

AI.TASKDEFINE("analisi", "analizza i requisiti")
AI.TASKDEFINE("sintesi", "sintetizza la soluzione")
AI.TASKDEPENDS("sintesi", "analisi")
AI WORKER MANAGE , 4 REQUIRE "LEN(RESULT$) > 5"
PRINT AI.STATELOAD("mgr", "manage_exit")     ' 1 = tutti i task done
PRINT AI.TASKSTATUS("sintesi")                ' {"…","status":"done",…}


## 4-quinquies. AI REPLAY AUTO (E1: riesecuzione automatica degli eventi)

AI.REPLAYSTART("s1")                 ' avvia la registrazione degli eventi (funzioni esistenti)
ok = AI.REPLAYRECORD("1 + 1")        ' evento = espressione BASIC in forma testuale
ok = AI.REPLAYSTOP()

AI REPLAY AUTO "s1"                  ' COMANDO: riesegue automaticamente TUTTI gli eventi
j$ = AI.REPLAYGET("s1#auto")         ' esiti per-evento: "ok: …" / "error: …"


## 5. UI semantica (L1, AX)

PRINT UI.FIND("File")
IF UI.CLICK("Save") THEN PRINT "premuto"
UI.TYPE("Campo utente", "mario")
PRINT UI.FINDALL("File")   ' JSON-array [{role,title,level}] — "[]" se nessun match


## 6. ToolRegistry dinamico

PRINT TOOL.SEARCH("browser")   ' JSON [{name,description,provider,risk}]


## 7. Loop Brain→Azione

## 8. Capability e sicurezza

## 9. Backend e parità
| Funzione | IL | PCodeVM | VM/C | C99Native |
|---|---|---|---|---|
| BROWSER.* | ✅ C# | ✅ | ✅ C | ✅ C |
| BROWSER.ACCESSTREE() | ✅ | ✅ | ✅ | ✅ |
| stealth / ref-click @eN / profilo | ✅ | ✅ | ❌ | ❌ |
| COMPUTER.* (CLICK/MOVE/TYPE/KEY/DOUBLECLICK/HOTKEY/SCREENSHOT/WINDOWS) | ✅ C# | ✅ | ✅ C | ✅ C |
| COMPUTER.SCREENRECORD / SCREENRECORDSTART / SCREENRECORDSTOP / SCREENRECORDSTATUS | ✅ | ✅ | ✅ C (2024-2027 → bridge, 2026-09-13) | ✅ C |
| UI.* AX | ✅ C# | ✅ | ✅ C | ✅ C (2026-09-14: FINDALL/TYPE ai tipi dichiarati) |
| TOOL.SEARCH | ✅ C# | ✅ | ✅ C | ✅ |
| AGENT.RUN / AI.AGENTRUN | ✅ | ✅ | ✅ | ⏳ |

## 10. Esempi

## 11. Debug e limiti noti

<!-- END SURFACE -->








<!-- BEGIN DOMAIN TABLE -->
## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **BROWSER.ACCESSTREE()** — `PRINT "ax="; BROWSER.ACCESSTREE()` (shared/samples/Ai/BrowserAccessTreeClickFill.bas:12)
- **BROWSER.AUTH(x1, x2)** — `DIM list0$ = BROWSER.AUTH("list", "")` (shared/samples/Browser/BrowserGovernanceDemo.bas:53)
- **BROWSER.CLEARCOOKIES()** — `PRINT "clearcookies="; BROWSER.CLEARCOOKIES()` (shared/samples/Ai/BrowserActions.bas:34)
- **BROWSER.CLICK(x1)** — `DIM clickRes$ = BROWSER.CLICK("#btn1")` (shared/samples/Browser/BrowserGovernanceDemo.bas:36)
- **BROWSER.CONSOLE()** — `DIM con$ = BROWSER.CONSOLE(TRUE)` (shared/samples/Browser/BrowserGovernanceDemo.bas:47)
- **BROWSER.EVALUATE(x1)** — `PRINT "title: "; BROWSER.EVALUATE("document.title")` (shared/samples/Browser/BrowserGovernanceDemo.bas:38)
- **BROWSER.FILL(x1, x2)** — `PRINT "fill="; BROWSER.FILL("@e3","hello")` (shared/samples/Ai/BrowserAccessTreeClickFill.bas:16)
- **BROWSER.FIND(x1)** — `DIM f1$ = BROWSER.FIND("Hello GMBasic")` (shared/samples/Browser/BrowserGovernanceDemo.bas:29)
- **BROWSER.GETCOOKIES()** — `PRINT "=== BROWSER.GETCOOKIES ==="` (shared/samples/Browser/BrowserGetCookies.bas:11)
- **BROWSER.HUMANCLICK(x1)** — `DIM s9$ = BROWSER.HUMANCLICK("#id")` (shared/minitests/browser/browser_missing_symbols.bas:17)
- **BROWSER.HUMANFILL(x1, x2)** — `PRINT "humanfill="; BROWSER.HUMANFILL("input#non-esiste", "ciao")` (shared/samples/Ai/BrowserActions.bas:35)
- **BROWSER.HUMANSCROLL(x1)** — `PRINT BROWSER.HUMANSCROLL("")` (shared/minitests/browser/gen_BROWSER_HumanScroll.bas:4)
- **BROWSER.NAVIGATE(x1)** — `BROWSER.NAVIGATE("file://../../../samples/Fixtures/browser-eval/test_basic.html")` (shared/samples/Browser/BrowserGovernanceDemo.bas:20)
- **BROWSER.NETLOG()** — `DIM net$ = BROWSER.NETLOG(TRUE)` (shared/samples/Browser/BrowserGovernanceDemo.bas:45)
- **BROWSER.OBSERVE()** — `DIM obs$ = BROWSER.OBSERVE()` (shared/samples/Browser/BrowserGovernanceDemo.bas:23)
- **BROWSER.SCREENSHOT()** — `_b16 = BROWSER.Screenshot("")` (shared/minitests/browser/browser_all.bas:30)
- **BROWSER.SETCOOKIE(x1, x2)** — `DIM s6$ = BROWSER.SETCOOKIE("n", "v", "http://localhost/")` (shared/minitests/browser/browser_missing_symbols.bas:14)
- **BROWSER.SETHEADER(x1, x2)** — `_b18 = BROWSER.SetHeader("", "")` (shared/minitests/browser/browser_all.bas:32)
- **BROWSER.SETUSERAGENT(x1)** — `PRINT "=== BROWSER.SETUSERAGENT ==="` (shared/samples/Browser/BrowserSetUserAgent.bas:10)
- **BROWSER.SNAPSHOT()** — `snap$ = BROWSER.SNAPSHOT()` (shared/samples/Ai/BrowserActions.bas:28)
- **BROWSER.START()** — `BROWSER.START(%AUTO, 9222)` (shared/samples/Browser/BrowserGovernanceDemo.bas:17)
- **COMPUTER.CLICK(x1, x2)** — `PRINT "tools_computer="; Has(tc, "computer.click") <> 0` (shared/samples/cua/CuaDiagnostics.bas:38)
- **COMPUTER.DOUBLECLICK(x1, x2)** — `d = COMPUTER.DOUBLECLICK(0, 0)` (shared/samples/Ai/ComputerInput.bas:44)
- **COMPUTER.HOTKEY(x1)** — `r1 = COMPUTER.HOTKEY("hyper+z")` (shared/samples/Ai/ComputerInput.bas:52)
- **COMPUTER.KEY(x1)** — `PRINT COMPUTER.KEY("")` (shared/minitests/computer/gen_COMPUTER_Key.bas:5)
- **COMPUTER.MOVE(x1, x2)** — `PRINT COMPUTER.MOVE(0, 0)` (shared/minitests/computer/gen_COMPUTER_Move.bas:5)
- **COMPUTER.SCREENRECORD()** — `PRINT "=== COMPUTER.SCREENRECORD ==="` (shared/samples/Computer/ComputerScreenRecord.bas:10)
- **COMPUTER.SCREENRECORDSTART(x1)** — `DIM start$ AS STRING = COMPUTER.SCREENRECORDSTART(path$, fps)` (shared/samples/Ai/ComputerScreenRecord.bas:14)
- **COMPUTER.SCREENRECORDSTATUS()** — `DIM st$ AS STRING = COMPUTER.SCREENRECORDSTATUS()` (shared/samples/Ai/ComputerScreenRecord.bas:25)
- **COMPUTER.SCREENRECORDSTOP()** — `DIM stop$ AS STRING = COMPUTER.SCREENRECORDSTOP()` (shared/samples/Ai/ComputerScreenRecord.bas:21)
- **COMPUTER.SCREENSHOT()** — `shot$ = COMPUTER.SCREENSHOT("work/computer_shot.png")` (shared/samples/Ai/ComputerUse.bas:15)
- **COMPUTER.TYPE(x1)** — `PRINT "=== COMPUTER.TYPE ==="` (shared/samples/Computer/ComputerType.bas:9)
- **COMPUTER.WINDOWS()** — `win = COMPUTER.WINDOWS()` (shared/samples/cua/AgentWorkflow.bas:31)
<!-- END DOMAIN TABLE -->
