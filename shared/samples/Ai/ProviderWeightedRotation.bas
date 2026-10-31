' FILE: AI/ProviderWeightedRotation.bas
' =================================================================
' Rotazione pesata tra provider di pari priorita': AI.SETWEIGHT.
' Finche' nessun peso viene impostato, AI.ROTATE si comporta come
' round-robin semplice (comportamento invariato, zero rischio per il codice
' esistente). Solo impostando pesi diversi da 1 si attiva la distribuzione
' proporzionale (smooth weighted round-robin, stesso algoritmo di nginx).
' =================================================================
IMPORT AI WITH ALIAS

SUB Main()
    PRINT "Provider Weighted Rotation Test"
    PRINT "================================"
    PRINT

    ' --- 1. Due provider di pari priorita', pesi diversi (3:1) ---
    AI.REGISTER("completion", "heavy", "fixture", """{}""")
    AI.REGISTER("completion", "light", "fixture", """{}""")
    AI.SETWEIGHT("heavy", 3)
    AI.SETWEIGHT("light", 1)
    ' Il provider di default (fixture-completion) resta nel pool: lo blocchiamo
    ' per osservare solo la distribuzione tra 'heavy' e 'light'.
    AI.SETSTATUS("fixture-completion", "BLOCKED")
    PRINT "heavy peso="; AI.WEIGHT("heavy"); "  light peso="; AI.WEIGHT("light")
    PRINT

    ' --- 2. 8 rotazioni: atteso 6 volte 'heavy', 2 volte 'light' (rapporto 3:1) ---
    PRINT "--- 8 chiamate a ROTATE ---"
    DIM heavyCount AS INTEGER = 0
    DIM lightCount AS INTEGER = 0
    FOR i = 1 TO 8
        DIM picked$ = AI.ROTATE("completion")
        PRINT "  giro "; i; ": "; picked$
        IF picked$ = "heavy" THEN heavyCount = heavyCount + 1
        IF picked$ = "light" THEN lightCount = lightCount + 1
    NEXT i
    PRINT
    PRINT "heavy scelto "; heavyCount; " volte, light scelto "; lightCount; " volte (atteso 6:2)"
    PRINT

    ' --- 3. La priorita' vince sempre sul peso ---
    AI.REGISTER("completion", "top-priority", "fixture", """{}""")
    AI.SETPRIORITY("top-priority", 100)
    PRINT "Con top-priority (priorita' 100, peso di default), ROTATE lo sceglie sempre:"
    PRINT "  "; AI.ROTATE("completion")
    PRINT "  "; AI.ROTATE("completion")

    PRINT
    PRINT "Test Completato."
END SUB
