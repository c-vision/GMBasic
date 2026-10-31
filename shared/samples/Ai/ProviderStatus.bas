' FILE: AI/ProviderStatus.bas
' =================================================================
' PROVIDER status (Available/Unavailable/Blocked/Damaged) + priorita'
' + health check in background.
' =================================================================
IMPORT AI WITH ALIAS

SUB Main()
    PRINT "Provider Status & Health Check Test"
    PRINT "===================================="
    PRINT

    ' --- 1. Priorita': provider a priorita' piu' alta e' preferito ---
    AI.REGISTER("embedding", "p-high", "fixture", """{}""")
    AI.SETPRIORITY("p-high", 100)
    AI.REGISTER("embedding", "p-mid", "fixture", """{}""")
    AI.SETPRIORITY("p-mid", 50)

    PRINT "Stato iniziale p-high: "; AI.STATUS("p-high")
    PRINT "Provider attivo (priorita' piu' alta vince): "; AI.ACTIVE("embedding")
    PRINT

    ' --- 2. Stato manuale: BLOCKED esclude dalla rotazione ---
    PRINT "--- Blocco manuale ---"
    AI.SETSTATUS("p-high", "BLOCKED")
    PRINT "p-high status: "; AI.STATUS("p-high")
    PRINT "Provider attivo dopo blocco (salta p-high): "; AI.ACTIVE("embedding")
    PRINT

    ' --- 3. Stato manuale: DAMAGED esclude anch'esso dalla rotazione ---
    PRINT "--- Danneggiato manuale ---"
    AI.SETSTATUS("p-mid", "DAMAGED")
    PRINT "p-mid status: "; AI.STATUS("p-mid")
    ' A questo punto solo fixture-embedding (priorita' 0) e' disponibile
    PRINT "Provider attivo (solo fixture-embedding disponibile): "; AI.ACTIVE("embedding")
    PRINT

    ' --- 4. Ripristino: solo AVAILABLE riabilita la rotazione ---
    AI.SETSTATUS("p-high", "AVAILABLE")
    AI.SETSTATUS("p-mid", "AVAILABLE")
    PRINT "Dopo ripristino, provider attivo: "; AI.ACTIVE("embedding")
    PRINT

    ' --- 5. Health check in background ---
    PRINT "--- Health check background ---"
    PRINT "Intervallo di default: "; AI.HEALTHCHECKINTERVAL(); " minuti"
    AI.STARTHEALTHCHECK(10)  ' 0 = usa il default (10 minuti)
    PRINT "Health check avviato, intervallo: "; AI.HEALTHCHECKINTERVAL(); " minuti"
    ' Il servizio testa ogni provider ad ogni tick e marca Available/Unavailable;
    ' non tocca mai i provider in stato Blocked o Damaged (override manuale).
    AI.STOPHEALTHCHECK()
    PRINT "Health check fermato."

    PRINT
    PRINT "Test Completato."
END SUB
