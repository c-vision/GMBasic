' FILE: AI/ProviderFailover.bas
' =================================================================
' PROVvider failover automatico + priorita
' =================================================================
IMPORT SQL WITH ALIAS
IMPORT AI WITH ALIAS

SUB Main()
    PRINT "Provider Failover & Priority Test"
    PRINT "================================"
    PRINT
    
    ' --- 1. Registra piu provider per lo stesso ruolo ---
    PRINT "--- Registrazione provider ---"
    AI.REGISTER("embedding", "primary-emb", "fixture", "{""dimensions"":""8""}")
    AI.SETPRIORITY("primary-emb", 100)
    PRINT "Registrato primary-emb (priority 100)"
    
    AI.REGISTER("embedding", "secondary-emb", "fixture", "{""dimensions"":""8""}")
    AI.SETPRIORITY("secondary-emb", 50)
    PRINT "Registrato secondary-emb (priority 50)"
    
    AI.REGISTER("embedding", "tertiary-emb", "fixture", "{""dimensions"":""8""}")
    AI.SETPRIORITY("tertiary-emb", 10)
    PRINT "Registrato tertiary-emb (priority 10)"
    PRINT
    
    ' --- 2. Provider attivo (dovrebbe essere primary per priorita') ---
    PRINT "Provider attivo: "; AI.ACTIVE("embedding")
    DIM test1$ = AI.TEST("embedding", AI.ACTIVE("embedding"))
    PRINT "Test: "; test1$
    PRINT
    
    ' --- 3. Simula fallimento del primario ---
    PRINT "--- Failover test ---"
    PRINT "Marking primary as FAILED..."
    AI.FAIL("primary-emb")
    
    ' Il sistema dovrebbe passare a secondary-emb (priorita' 50)
    PRINT "Provider attivo dopo fail: "; AI.ACTIVE("embedding")
    DIM test2$ = AI.TEST("embedding", AI.ACTIVE("embedding"))
    PRINT "Test: "; test2$
    PRINT
    
    ' --- 4. Fallimento anche del secondary ---
    PRINT "Marking secondary as FAILED..."
    AI.FAIL("secondary-emb")
    
    ' Ora dovrebbe usare tertiary-emb
    PRINT "Provider attivo dopo secondo fail: "; AI.ACTIVE("embedding")
    DIM test3$ = AI.TEST("embedding", AI.ACTIVE("embedding"))
    PRINT "Test: "; test3$
    PRINT
    
    ' --- 5. Ripristina tutti ---
    AI.RESETFAILED("")
    PRINT "Provider dopo reset: "; AI.ACTIVE("embedding")
    PRINT
    
    ' --- 6. Verifica prioritie' ---
    PRINT "--- Priorita' ---"
    PRINT "primary priority: "; AI.PRIORITY("primary-emb")
    PRINT "secondary priority: "; AI.PRIORITY("secondary-emb")
    PRINT "tertiary priority: "; AI.PRIORITY("tertiary-emb")
    PRINT
    
    ' --- 7. Uso pratico con vector search ---
    PRINT "--- Vector search con provider failover ---"
    ' P0 (decisione 2026-09-12): AI RAG senza provider embedding attivo = errore esplicito.
    ' Attivazione canonica del provider (fixture scelto ESPPLICITAMENTE = legittimo).
    AI SET ACTIVE "embedding", "primary-emb"
    PRINT "Embedding attivo per AI RAG: "; AI.ACTIVE("embedding")
    SQL OPEN ":memory:" ALIAS faildb
    AI RAG ENSURE COLLECTION "faildb", "docs", 8
    AI RAG INDEX TEXT "faildb", "docs", "doc1", "Il failover dei provider e' automatico."
    DIM res$ = AI.SEARCH("faildb", "docs", "failover", 3)
    PRINT "Search: "; res$
    PRINT
    
    PRINT "Test Completato."
END SUB
