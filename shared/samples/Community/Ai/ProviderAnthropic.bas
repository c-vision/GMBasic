' PARITY-EXCLUDE: env: il C# fa HTTP 401 reale con chiave invalida
' FILE: AI/ProviderAnthropic.bas
' =================================================================
' Provider completion Anthropic (Claude) — solo completion, Anthropic
' non espone una API di embeddings pubblica.
' =================================================================
IMPORT AI WITH ALIAS

SUB Main()
    PRINT "Provider Anthropic Test"
    PRINT "========================"
    PRINT

    ' --- 1. Registrazione: serve una vera ANTHROPIC_API_KEY per funzionare davvero ---
    AI.REGISTER("completion", "claude", "anthropic", _
        """{"apiKey":"sk-ant-demo","model":"claude-sonnet-5"}""")
    PRINT "Registrato 'claude' (kind=anthropic, model=claude-sonnet-5)"
    PRINT

    ' --- 2. Solo COMPLETION: 'anthropic' come kind embedding non e' supportato ---
    PRINT "Kind 'anthropic' e' valido solo per completion, non per embedding."
    PRINT

    ' --- 3. Selezione esplicita e priorita' ---
    AI.SETPRIORITY("claude", 100)
    AI.SETACTIVE("completion", "claude")
    PRINT "Provider completion attivo: "; AI.ACTIVE("completion")
    PRINT

    ' --- 4. Test di connettivita' (fallisce senza una vera API key/rete) ---
    PRINT "--- Test connettivita' ---"
    DIM result$ = AI.TEST("completion", "claude")
    PRINT result$
    PRINT

    PRINT "Test Completato."
END SUB
