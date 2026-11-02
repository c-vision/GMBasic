' MiniTest: AI DECIDE v0 (JEV/SystemOne) — provider Rules deterministico, JSON su 4 gambe.
' La superficie tipizzata DECISION<T> e il provider locale (ABI v8 CAP_DECIDE) sono il passo
' successivo; qui si verifica il contratto/provider e la parità del JSON.
' EXPECT: noul-true:{"kind":"NOUL","option":"true","value":"true","probability":1.000000,"confidence":null,"calibrated":null}|noul-false:{"kind":"NOUL","option":"false","value":"false","probability":0.500000,"confidence":null,"calibrated":null}|score:{"kind":"SCORE","option":"0","value":"0","probability":0.500000,"confidence":null,"calibrated":null}|choice:{"kind":"CHOICE","option":"A","value":"A","probability":0.500000,"confidence":null,"calibrated":null}
IMPORT AI

SUB Main()
    DIM r AS STRING
    AI DECIDE "il cliente ha detto SÌ", "NOUL" INTO r
    PRINT "noul-true:"; r
    AI DECIDE "nulla di rilevante", "NOUL" INTO r
    PRINT "noul-false:"; r
    AI DECIDE "x", "SCORE" INTO r
    PRINT "score:"; r
    AI DECIDE "x", "CHOICE" INTO r
    PRINT "choice:"; r
END SUB
