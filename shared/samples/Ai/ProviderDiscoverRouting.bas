' PARITY-EXCLUDE: rete (discovery OpenRouter: conteggi/modelli remoti variano per run: 25 vs 455 vs 1) + rotazione su provider remoti (2026-09-23, Parita-5-Backend-Residui §2B)
' FILE: AI/ProviderDiscoverRouting.bas
' =================================================================
' AI DISCOVER / AI SET ACTIVE / AI.SETCREDITS — carica i modelli di un'API key
' (OpenRouter), li marca free/a pagamento, e fa credit-based routing con priorita'.
' =================================================================
' SINTASSI (direttiva): getter = funzione, setter = comando.
'   Funzione:  modelli() = AI.DISCOVER("or", %CHAT, url, key, TRUE)
'   Comando:   AI SET ACTIVE %CHAT, nome   |  AI SET PRIORITY nome, 100
'   Comando:   AI SET CREDITS nome, 5   (legacy)
'   Funzione:  nome$ = AI.ACTIVE(%CHAT)    |  AI.CREDITS(nome$)
'
' REQUISITI:
'   - API key OpenRouter valida (sostituisci openrouterKey qui sotto)
'   - Con una chiave valida, DISCOVER registra ~400 modelli come
'     "or:<model-id>" (i free con priorita' 10, free-first).
'
' USO:
'   1. Sostituisci la chiave in openrouterKey (dentro SUB Main) con la tua chiave reale
'   2. Esegui:  dotnet run --project REPL/REPL.csproj -- samples/ai/ProviderDiscoverRouting.bas -n
'   3. Con una chiave valida farai ~1 chiamata HTTP reale a OpenRouter.
'      (usa -n per il backend IL nativo: il VM/C non ha ancora i builtin DISCOVER/CREDITS)
IMPORT AI WITH ALIAS

SUB Main()
    ' Nota: variabili locali (le costanti stringa globali non sono ancora
    ' supportate dal backend IL nativo di gm).
    DIM openrouterKey AS STRING
    DIM openrouterUrl AS STRING
    openrouterKey = "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"
    openrouterUrl = "https://openrouter.ai/api/v1"

    PRINT "OpenRouter Discover & Credit Routing Test"
    PRINT "========================================="
    PRINT

    ' --- 1. Scopri tutti i modelli free dell'API key ---
    '     (5° argomento soloFree=TRUE: registra solo i modelli free)
    '     %CHAT/%OCR/%AUDIO sono costanti numeriche (0/1/2) predefinite.
    '     DISCOVER è un comando (azione: scopre e registra); la lettura è AI.LIST(role).
    AI DISCOVER "or", %CHAT, openrouterUrl, openrouterKey, TRUE
    DIM modelli() = AI.LIST(%CHAT)

    PRINT "Modelli free scoperti: "; UBOUND(modelli) + 1
    PRINT "Primi 10:"
    FOR i = 0 TO MIN(9, UBOUND(modelli))
        PRINT "  "; modelli(i)
    NEXT i
    PRINT

    ' --- 2. I free hanno gia' priorita' 10; evidenziamo il primo ---
    IF UBOUND(modelli) >= 0 THEN
        AI SET PRIORITY modelli(0), 100
        AI SET ACTIVE %CHAT, modelli(0)
        PRINT "Provider chat attivo: "; AI.ACTIVE(%CHAT)
    END IF
    PRINT

    ' --- 3. Credit-based routing ---
    '     Imposta un budget di crediti gratuiti; a esaurimento il provider
    '     va BLOCKED e la rotazione passa al successivo.
    DIM nome$ AS STRING
    nome$ = AI.ACTIVE(%CHAT)
    IF LEN(nome$) > 0 THEN
        AI SET CREDITS nome$, 5
        PRINT "Crediti impostati per "; nome$; ": "; AI.CREDITS(nome$)
        PRINT

        ' --- 4. Simula completamenti fino a esaurimento crediti ---
        '     (In un uso reale RecordCompletion e' chiamato automaticamente dal
        '      runtime a ogni risposta; qui mostriamo il comportamento simulato.)
        FOR i = 1 TO 6
            IF i <= 5 THEN
                AI SET CREDITS nome$, 5 - i
                PRINT "Completamento "; i; " -> crediti: "; AI.CREDITS(nome$)
            ELSE
                AI SET CREDITS nome$, 0
                PRINT "Completamento "; i; " -> crediti: "; AI.CREDITS(nome$); " (BLOCKED)"
            END IF
        NEXT i

        PRINT
        PRINT "Stato provider dopo esaurimento: "; AI.STATUS(nome$)
        PRINT "Provider successivo (rotazione): "; AI.ACTIVE(%CHAT)
    ELSE
        PRINT "Nessun provider attivo: chiave non valida o nessun modello free."
    END IF

    PRINT
    PRINT "Test completato."
END SUB
