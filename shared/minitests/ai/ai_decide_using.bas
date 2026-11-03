' MiniTest: clausola canonica USING nella forma SINGOLA di AI DECIDE
' (parità con la forma batch; il provider a comma resta supportato).
' EXPECT: using-rules={"kind":"NOUL","option":"true","value":"true","probability":1.000000,"confidence":null,"calibrated":null}|using-mock={"kind":"NOUL","option":"false","value":"false","probability":1.000000,"confidence":null,"calibrated":null}|using-fb={"kind":"NOUL","option":"true","value":"true","probability":1.000000,"confidence":null,"calibrated":null}
IMPORT AI

SUB Main()
    DIM r AS STRING
    AI DECIDE "sì", "NOUL" USING "rules" INTO r
    PRINT "using-rules="; r
    AI DECIDE "sì", "NOUL" USING "mock" INTO r
    PRINT "using-mock="; r
    AI DECIDE "sì", "NOUL" USING "local" FALLBACK "rules" INTO r
    PRINT "using-fb="; r
END SUB
