' MiniTest: AI DECIDE con catena di fallback ESPLICITA (FALLBACK "p1","p2").
' Il primo provider che non risponde errore vince; senza modello il local fallisce e la
' catena prosegue (nessun fallback implicito: solo quello scritto dal programma).
' EXPECT: fb-rules={"kind":"NOUL","option":"true","value":"true","probability":1.000000,"confidence":null,"calibrated":null}|fb-mock={"kind":"NOUL","option":"false","value":"false","probability":1.000000,"confidence":null,"calibrated":null}|fb-chain={"kind":"NOUL","option":"true","value":"true","probability":1.000000,"confidence":null,"calibrated":null}|fb-primary={"kind":"NOUL","option":"false","value":"false","probability":1.000000,"confidence":null,"calibrated":null}|fb-fail={"error":"provider 'bogus' non valido (rules/mock/local)"}|fb-batch=true/ 1
IMPORT AI

SUB Main()
    DIM r AS STRING
    AI DECIDE "sì", "NOUL", "local" FALLBACK "rules" INTO r
    PRINT "fb-rules="; r
    AI DECIDE "sì", "NOUL", "local" FALLBACK "mock", "rules" INTO r
    PRINT "fb-mock="; r
    AI DECIDE "sì", "NOUL" FALLBACK "local", "rules" INTO r
    PRINT "fb-chain="; r
    AI DECIDE "no", "NOUL", "mock" FALLBACK "rules" INTO r
    PRINT "fb-primary="; r
    AI DECIDE "x", "NOUL" FALLBACK "bogus" INTO r
    PRINT "fb-fail="; r

    DIM a AS DECISION
    AI DECIDE
        a AS NOUL
    FROM "sì"
    USING "local"
    FALLBACK "rules"
    END DECIDE
    PRINT "fb-batch="; a.VALUE; "/"; a.PROBABILITY
END SUB
