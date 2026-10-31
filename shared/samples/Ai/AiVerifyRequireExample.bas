' FILE: Ai/AiVerifyRequireExample.bas
' =================================================================================================
'   ESEMPIO — AI VERIFY <attempt> REQUIRE <requireExpr> [INTO <var>]   (comando, 2026-09-09)
'
'   Il comando esegue un "verifier/retry": ogni TENTATIVO produce un risultato; se il REQUIRE
'   (un'ESPRESSIONE BASIC valutata sul risultato) non dà TRUE si ritenta (fino a 3 volte).
'   - attempt  : espressione stringa con l'AZIONE da eseguire. v1: "COMPILE <file.bas>"
'                (compila un file; deterministico, nessun LLM richiesto).
'   - require  : espressione stringa valutata con le LOCALI del chiamante + RESULT$ (il
'                risultato dell'ultimo tentativo riuscito). Deve dare un valore truthy.
'   - INTO <v> : (opzionale) scrive il risultato finale accettato nella variabile v (AS STRING).
'   Senza INTO il comando è uno statement puro. Su fallimento dopo i retry: errore ESPLICITO.
'   Backend: IL e PCodeVM. VM/C, C99Native NON lo supportano (l'azione COMPILE
'   vive nell'host C#, mai bridge C<->C#).
' =================================================================================================
IMPORT AI WITH ALIAS

SUB Main()
    DIM ok AS INTEGER
    ok = 0

    ' --- 1. compile che RIUSCE + REQUIRE su RESULT$ e su una locale del chiamante ---
    DIM esito AS STRING
    DIM soglia AS INTEGER
    soglia = 8
    AI VERIFY "COMPILE ../../../samples/Fixtures/AIVerifyFixtureOk.bas" REQUIRE "LEN(RESULT$) > soglia" INTO esito
    IF esito = "Compile OK" THEN ok = ok + 1 ELSE PRINT "req-ex-FAIL1:"; esito

    ' --- 2. stesso REQUIRE ma come statement puro (senza INTO): l'esito è scartato ---
    AI VERIFY "COMPILE ../../../samples/Fixtures/AIVerifyFixtureOk.bas" REQUIRE "LEN(RESULT$) > 0"
    ok = ok + 1

    ' NB: su fallimento (compile KO o REQUIRE mai soddisfatto dopo 3 tentativi) il comando
    ' termina con un errore ESPLICITO "AI VERIFY fallito dopo 3 tentativi" — mai silenzioso.
    ' (La consegna dell'errore sul backend PCode è asincrona dal park: non va catturato con
    ' TRY/CATCH in questo giro — vedi canarino di diagnostica per il messaggio.)

    PRINT "req-ex-cases:"; ok
    IF ok = 2 THEN PRINT "req-ex-ALL-OK" ELSE PRINT "req-ex-FAIL"
END SUB
