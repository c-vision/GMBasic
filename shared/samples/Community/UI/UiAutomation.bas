' FILE: UI/UiAutomation.bas
' =================================================================================================
' UI automation L1 — FIND/FINDALL/CLICK/TYPE in un solo programma (R2 §0 todo MiniTest).
' Target che non matchano nulla: comportamento deterministico e senza side effect su
' tutte le gambe (headless/CI e AX disponibile: needle vuota non hitta mai).
'   UI.FIND("")      -> {"found":false}
'   UI.FINDALL("")   -> []
'   UI.CLICK("...")  -> 0  (nessun match: nessun click reale)
'   UI.TYPE("","")   -> 0  (nessun campo: nessuna digitazione reale)
' Con AX disponibile e target reali: FIND/FINDALL ritornano i match, CLICK/TYPE agiscono
' davvero (1/0) — vedi CUA-Universal-Action-Runtime-Guida §5.
' NOTA: PRINT dirette senza DIM intermedi (un bug ILOptim su assegnazione DIM INTEGER
' da call inquina stdout — vedi Bugs/(open) - ILOptim-ImportBinary-DimInteger-CallAssign).
' =================================================================================================
IMPORT UI

SUB Main()
    PRINT "=== UI automation L1 ==="
    PRINT "find: "; UI.FIND("")
    PRINT "findall: "; UI.FINDALL("")
    IF LEN(UI.FINDALL("")) = 2 THEN PRINT "findall-vuoto: si" ELSE PRINT "findall-vuoto: no"
    PRINT "click: "; UI.CLICK("gmb-elemento-inesistente-xyz")
    PRINT "type: "; UI.TYPE("", "")
    PRINT "click+type: "; UI.CLICK("gmb-elemento-inesistente-xyz") + UI.TYPE("", "")
    PRINT "=== OK ==="
END SUB
