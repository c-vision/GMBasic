' FILE: Ai/AiEvalValidateExample.bas
' =================================================================================================
'   AI EVAL VALIDATE <result> AGAINST [VAULT] <folder> [INTO <var$>]   (comando, 2026-09-09, T4.1)
'   UAT automatica via vault: valida un RISULTATO contro i DOCUMENTI DI REQUISITO di una cartella
'   (oracolo a termini, nessun LLM). INTO riceve il verdetto JSON:
'     {"ok":bool,"passed":[...],"failed":[...],"coverage":n}
'   Host C# (VaultValidator): VM/C, C99Native NON lo supportano (N/A documentato).
' =================================================================================================
IMPORT AI WITH ALIAS

SUB Main()
    DIM dirUat AS STRING
    dirUat = "../../Fixtures/UatVault"

    ' 1. risultato CONFORME -> ok:true
    DIM jsonOk AS STRING
    AI EVAL VALIDATE "the system must support user login and it must calculate the order total" AGAINST VAULT dirUat INTO jsonOk
    PRINT "eval-json-ok:"; jsonOk
    IF INSTR(jsonOk, ":true") > 0 THEN PRINT "eval-ok1" ELSE PRINT "eval-FAIL1:"; jsonOk

    ' 2. risultato che NON copre un requisito -> ok:false e failed popolato
    DIM jsonBad AS STRING
    AI EVAL VALIDATE "the component calculates the order total but the authentication flow is missing" AGAINST VAULT dirUat INTO jsonBad
    PRINT "eval-json-bad:"; jsonBad
    IF INSTR(jsonBad, ":false") > 0 AND INSTR(jsonBad, "login") > 0 THEN PRINT "eval-ok2" ELSE PRINT "eval-FAIL2:"; jsonBad

    PRINT "eval-done"
END SUB
