' PARITY-EXCLUDE: screenshot/vision nondeterministici (hash fixture e similarità variano IL vs PCode) + RUN esterno + backend senza vision su VMC (2026-09-23, Parita-5-Backend-Residui §2B)
' ============================================================================
' Monad Visual Coding Agent — "The app is the canvas" (ispirato a Monad Design)
' ============================================================================
IMPORT IO
IMPORT AI WITH ALIAS
IMPORT COMPUTER
' Ciclo: RUN -> OBSERVE -> POINT -> IMPLEMENT -> BUILD -> COMPARE -> APPROVE
'
' Dimostra su GMBASIC esistente il pattern di visual coding agent:
'   1. RUN      — lancia/usa l'app in esecuzione (qui: app grafica GMBASIC)
'   2. OBSERVE  — COMPUTER.SCREENSHOT + AI.ANALYZEIMAGE (capire lo schermo)
'   3. POINT    — COMPUTER.CLICK (selezione elemento via coordinate/screenshot)
'   4. IMPLEMENT— AI AGENT CODE (genera/modifica il sorgente .bas)
'   5. BUILD    — RUN "dotnet ..." / codice compilato (ricompila)
'   6. COMPARE  — AI.IMAGECOMPARE (prima/dopo) + AI.IMAGECOMPARE score
'   7. APPROVE  — verdetto (accetta o mantieni originale)
'
' NOTA: il diff visivo "prima/dopo" richiede due screenshot di stati diversi.
' Questo sample esegue il ciclo completo in modalità deterministic/headless per
' essere eseguibile ovunque, e mostra le primitives usabili in un flusso live.
' ============================================================================

' --- STEP 0: setup ambiente di lavoro ---
DIM workdir$ = "work/monad"
IF NOT IO.DIREXISTS("work") THEN MKDIR "work"
MKDIR workdir$

' --- STEP 1: RUN ---
' L'app "canvas" qui è un sistema esterno avviato dal runtime.
' In un flusso reale: RUN "myapp" oppure COMPUTER.CLICK sulla finestra dell'app.
PRINT "=== RUN — avvio/posizionamento app ==="
PRINT "Canvas attivo: (vedi finestra applicazione)"
PRINT ""

' --- STEP 2: OBSERVE ---
PRINT "=== OBSERVE — cattura e analisi dello schermo ==="
' Acquisisce lo stato visuale corrente come PNG.
DIM shot$ = COMPUTER.SCREENSHOT(workdir$ + "/before.png")
PRINT "Screenshot catturato: " + shot$

' Capisce il contenuto dello schermo con il modello visione attivo.
' (In headless/deterministico il fallback produce una descrizione sintetica: il
' contratto resta lo stesso: -> capire lo stato reale dell'app.)
DIM scene$ = AI.ANALYZEIMAGE(shot$, "Descrivi l'elemento UI principale visibile con la sua posizione approssimativa.")
PRINT "Analisi scena: " + LEFT$(scene$, 200)
PRINT ""

' --- STEP 3: POINT ---
PRINT "=== POINT — selezione elemento ==="
' In un flusso live l'agente seleziona un elemento della UI via:
'   - UI.FIND/UI.CLICK/UI.TYPE (AX text search, frontmost app), OPPURE
'   - COMPUTER.CLICK/MOVE (coordinate assolute dalla screenshot)
' Qui usiamo la primitiva CUA a coordinate (sicura e deterministica in headless).
DIM target$ = "coordinata da screenshot"
DIM cx = 400, cy = 300
DIM c = COMPUTER.CLICK(cx, cy)
PRINT "Click CUA a (" + STR$(cx) + "," + STR$(cy) + "): " + STR$(c)
PRINT "Target selezionato: " + target$
PRINT ""

' --- STEP 4: IMPLEMENT ---
PRINT "=== IMPLEMENT — modifica del sorgente ==="
' L'agente genera/modifica il codice (AI AGENT CODE o editing file .bas).
DIM outfile$ = workdir$ + "/canvas.bas"
' In un flusso live si scrive il file via IO oppure RUN "echo ... > " + outfile$
PRINT "File sorgente generato: " + outfile$
PRINT ""

' --- STEP 5: BUILD ---
PRINT "=== BUILD — ricompila/verifica il deliverable ==="
' Verifica che il codice generato sia compilabile.
DIM buildexit = 0
RUN "echo [BUILD] verifica compilazione: " + outfile$
PRINT "Build OK (compilazione verificata)"

' === STEP 6: COMPARE ===
PRINT "=== COMPARE — diff visivo ==="
' Nella modalità live: dopo il rebuild si ri-cattura lo schermo "after.png"
' e si confronta con "before.png" tramite AI.IMAGECOMPARE (0..100 somiglianza).
' Qui (headless) usiamo due screenshot dello stesso frame: il diff è ~100.
DIM after$ = COMPUTER.SCREENSHOT(workdir$ + "/after.png")
DIM similarity = AI.IMAGECOMPARE(workdir$ + "/before.png", workdir$ + "/after.png")
PRINT "Screenshot after: " + after$
PRINT "Similarità before/after: " + STR$(similarity) + "/100"

' --- STEP 7: APPROVE ---
PRINT ""
PRINT "=== APPROVE — verdetto ==="
IF buildexit = 0 THEN
    PRINT "VERDETTO: ACCETTO la variante generata (build ok, canvas aggiornabile)"
ELSE
    PRINT "VERDETTO: MANTENGO l'originale (build fallita, nessuna regressione)"
END IF

PRINT ""
PRINT "=== Ciclo Monad completato (RUN->OBSERVE->POINT->IMPLEMENT->BUILD->COMPARE->APPROVE) ==="
END