' MiniTest: capability %DECIDE (ruolo registry append-only, valore 6).
' AI REGISTER alias, %DECIDE, "mock"|"rules" + AI SET ACTIVE %DECIDE, alias:
' AI DECIDE senza provider usa il provider di decisione attivo; un provider esplicito vince.
' EXPECT: role-mock={"kind":"NOUL","option":"false","value":"false","probability":1.000000,"confidence":null,"calibrated":null}|role-explicit={"kind":"NOUL","option":"true","value":"true","probability":1.000000,"confidence":null,"calibrated":null}|role-rules={"kind":"NOUL","option":"true","value":"true","probability":1.000000,"confidence":null,"calibrated":null}
IMPORT AI

SUB Main()
    DIM r AS STRING
    AI REGISTER "decmock", %DECIDE, "mock"
    AI SET ACTIVE %DECIDE, "decmock"
    AI DECIDE "sì", "NOUL" INTO r
    PRINT "role-mock="; r
    AI DECIDE "sì", "NOUL" USING "rules" INTO r
    PRINT "role-explicit="; r
    AI REGISTER "decrules", %DECIDE, "rules"
    AI SET ACTIVE %DECIDE, "decrules"
    AI DECIDE "sì", "NOUL" INTO r
    PRINT "role-rules="; r
END SUB
