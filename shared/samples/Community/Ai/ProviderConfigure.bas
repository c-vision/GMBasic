' FILE: AI/ProviderConfigure.bas
' =================================================================
' Gestione completa provider: register, configure, list, priority
' =================================================================
IMPORT AI WITH ALIAS

SUB Main()
    PRINT "Provider Management"
    PRINT "=================="
    PRINT
    
    ' --- 1. Lista provider iniziale ---
    PRINT "Provider embedding iniziali:"
    DIM embList() = AI.LIST("embedding")
    FOR i = 0 TO UBOUND(embList)
        PRINT "  "; embList(i)
    NEXT i
    PRINT
    
    ' --- 2. Registra provider con configurazione ---
    AI.REGISTER("embedding", "my-ollama", "ollama",
        """{"url":"http://localhost:11434","model":"nomic-embed-text"}""")
    PRINT "Registrato my-ollama (kind=ollama)"
    
    AI.REGISTER("embedding", "my-onnx", "onnx",
        """{"model":"/models/bge-small-onnx","dimensions":"384"}""")
    PRINT "Registrato my-onnx (kind=onnx)"
    PRINT
    
    ' --- 3. Configura provider esistente ---
    AI.CONFIGURE("my-ollama", "url", "http://ollama.local:11434")
    PRINT "Configurato my-ollama url -> http://ollama.local:11434"
    PRINT
    
    ' --- 4. Set priority ---
    AI.SETPRIORITY("my-onnx", 200)
    AI.SETPRIORITY("my-ollama", 100)
    PRINT "Priorita': my-onnx=200, my-ollama=100"
    PRINT
    
    ' --- 5. Lista dopo registrazione ---
    PRINT "Provider dopo registrazione:"
    DIM afterList() = AI.LIST("embedding")
    FOR i = 0 TO UBOUND(afterList)
        PRINT "  "; afterList(i); " (priority "; AI.PRIORITY(afterList(i)); ")"
    NEXT i
    PRINT
    
    ' --- 6. Test provider ---
    DIM testEmb$ = AI.TEST("embedding", "my-onnx")
    PRINT "Test my-onnx: "; testEmb$
    PRINT
    
    ' --- 7. Set attivo e verifica ---
    AI.SETACTIVE("embedding", "my-ollama")
    PRINT "Attivo dopo SETACTIVE: "; AI.ACTIVE("embedding")
    PRINT
    
    ' --- 8. Round-robin ---
    PRINT "Rotate: "; AI.ROTATE("embedding")
    PRINT "Rotate: "; AI.ROTATE("embedding")
    PRINT "Rotate: "; AI.ROTATE("embedding")
    PRINT
    
    ' --- 9. Unregister ---
    DIM removed = AI.UNREGISTER("my-ollama")
    PRINT "Unregister my-ollama: "; REMOVED
    
    AI.SETACTIVE("embedding", "my-onnx")
    PRINT "Attivo finale: "; AI.ACTIVE("embedding")
    PRINT
    
    ' --- 10. Save/Load configurazione ---
    AI.SAVE("/tmp/gmbasic-providers.json")
    PRINT "Config salvata in /tmp/gmbasic-providers.json"
    
    AI.REGISTER("embedding", "fresh-emb", "fixture", """{}""")
    PRINT "Registrato fresh-emb (non salvato)"
    
    AI.LOAD("/tmp/gmbasic-providers.json")
    PRINT "Dopo LOAD (dovrebbe aver perso fresh-emb):"
    DIM loadedList() = AI.LIST("embedding")
    FOR i = 0 TO UBOUND(loadedList)
        PRINT "  "; loadedList(i)
    NEXT i
    
    PRINT
    PRINT "Test Completato."
END SUB

ON ERROR RESUME NEXT
KILL "/tmp/gmbasic-providers.json"
KILL "Config salvata in /tmp/gmbasic-providers.json"
