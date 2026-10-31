' FILE: AI/ProviderHealthCheck.bas
' =================================================================
' PROVIDER health check in background: rilevamento automatico
' Available/Unavailable, esclusione dei provider Blocked/Damaged.
' =================================================================
IMPORT AI WITH ALIAS

SUB Main()
    PRINT "Provider Health Check Test"
    PRINT "=========================="
    PRINT

    ' --- 1. Un provider sano (fixture) e uno rotto (onnx con modello inesistente) ---
    AI.REGISTER("embedding", "healthy-emb", "fixture", """{}""")
    AI.SETPRIORITY("healthy-emb", 100)

    AI.REGISTER("embedding", "broken-emb", "onnx", _
        """{"model":"/tmp/does-not-exist.onnx","dimensions":"8"}""")
    AI.SETPRIORITY("broken-emb", 200)

    PRINT "Stato iniziale healthy-emb: "; AI.STATUS("healthy-emb")
    PRINT "Stato iniziale broken-emb: "; AI.STATUS("broken-emb")
    PRINT "Provider attivo (broken-emb ha priorita' piu' alta, ma non e' ancora stato testato): "; AI.ACTIVE("embedding")
    PRINT

    ' --- 2. Un provider bloccato manualmente: lo sweep non deve toccarlo ---
    AI.REGISTER("embedding", "blocked-emb", "fixture", """{}""")
    AI.SETSTATUS("blocked-emb", "BLOCKED")
    PRINT "blocked-emb status prima dello sweep: "; AI.STATUS("blocked-emb")
    PRINT

    ' --- 3. Esegui uno sweep di health check subito (senza attendere il timer) ---
    PRINT "--- Esecuzione AI.RUNHEALTHCHECK() ---"
    AI.RUNHEALTHCHECK()
    PRINT

    PRINT "Stato dopo lo sweep:"
    PRINT "  healthy-emb: "; AI.STATUS("healthy-emb"); " (atteso: AVAILABLE)"
    PRINT "  broken-emb:  "; AI.STATUS("broken-emb"); " (atteso: UNAVAILABLE, il modello onnx non esiste)"
    PRINT "  blocked-emb: "; AI.STATUS("blocked-emb"); " (atteso: BLOCKED, mai toccato dallo sweep)"
    PRINT

    ' --- 4. Il provider attivo ora salta broken-emb (Unavailable) e blocked-emb (Blocked) ---
    PRINT "Provider attivo dopo lo sweep (dovrebbe essere healthy-emb): "; AI.ACTIVE("embedding")
    PRINT

    PRINT "Test Completato."
END SUB
