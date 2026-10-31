' FILE: Ai/AiVerifyCanary.bas
' =================================================================================================
'   AI VERIFY <attempt> REQUIRE <require> (comando b, 2026-09-09) — verifier/retry col REQUIRE
'   valutato sul risultato del tentativo (RESULT$) + LOCALI del chiamante (S_eval). Azione v1:
'   "COMPILE <path>" (deterministico, host C# via CodeGenTools/Orchestrator). Esito su variabile
'   (INTO) o scartato. VM/C/C99: N/A (l'azione COMPILE vive nell'host C#, mai bridge).
' =================================================================================================
IMPORT AI WITH ALIAS
SUB Main()
    DIM expected AS STRING
    expected = "Compile OK"
    DIM n AS INTEGER
    n = 10

    ' 1. REQUIRE usa RESULT$ E una locale numerica del chiamante (parità S_eval)
    DIM res AS STRING
    AI VERIFY "COMPILE ../../../samples/Fixtures/AIVerifyFixtureOk.bas" REQUIRE "LEN(RESULT$) > n - 8" INTO res
    IF res = expected THEN PRINT "verify-ok1" ELSE PRINT "verify-FAIL1:"; res

    ' 2. REQUIRE con aritmetica su RESULT$ (len > 5)
    AI VERIFY "COMPILE ../../../samples/Fixtures/AIVerifyFixtureOk.bas" REQUIRE "LEN(RESULT$) > 5" INTO res
    IF res = expected THEN PRINT "verify-ok2" ELSE PRINT "verify-FAIL2:"; res

    ' 3. comando senza INTO: esito scartato (statement puro)
    AI VERIFY "COMPILE ../../../samples/Fixtures/AIVerifyFixtureOk.bas" REQUIRE "LEN(RESULT$) > 5"
    PRINT "verify-ok3"
END SUB
