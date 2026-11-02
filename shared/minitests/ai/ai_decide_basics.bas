' MiniTest: AI DECIDE v0 (JEV/SystemOne) — provider Rules deterministico, JSON su 4 gambe.
' La superficie tipizzata DECISION<T> e il provider locale (ABI v8 CAP_DECIDE) sono il passo
' successivo; qui si verifica il contratto/provider e la parità del JSON.
' EXPECT: noul-true:{"kind":"NOUL","option":"true","value":"true","probability":1.000000,"confidence":null,"calibrated":null}|noul-false:{"kind":"NOUL","option":"false","value":"false","probability":0.500000,"confidence":null,"calibrated":null}|score:{"kind":"SCORE","option":"0","value":"0","probability":0.500000,"confidence":null,"calibrated":null}|choice:{"kind":"CHOICE","option":"A","value":"A","probability":0.500000,"confidence":null,"calibrated":null}|choice-labels:{"kind":"CHOICE","option":"A","value":"billing","probability":0.500000,"confidence":null,"calibrated":null}|typed-value=billing|typed-prob= 0.5|typed-conf=[]
IMPORT AI
IMPORT JSON

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
    AI DECIDE "fattura", "CHOICE:billing|technical|sales" INTO r
    PRINT "choice-labels:"; r
    ' accesso tipizzato via JSON.PARSE (parità 4/4 dopo il fix case-insensitive del dict C)
    DIM d AS JSON = JSON.PARSE(r)
    PRINT "typed-value="; d.VALUE
    PRINT "typed-prob="; d.PROBABILITY
    PRINT "typed-conf=["; d.CONFIDENCE; "]"
END SUB
