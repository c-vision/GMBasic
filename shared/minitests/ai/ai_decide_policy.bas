' MiniTest: AI DECIDE produce EVIDENZA; la policy (soglia) resta nel programma, non nel provider.
' EXPECT: evidenza: true p= 1|policy: blocco
IMPORT AI

SUB Main()
    DIM fraud AS DECISION
    AI DECIDE
        fraud AS NOUL
    FROM "Il cliente ha detto sì: è una frode?"
    USING "rules"
    END DECIDE

    PRINT "evidenza: "; fraud.VALUE; " p="; fraud.PROBABILITY
    IF fraud.VALUE = "true" AND fraud.PROBABILITY >= 0.8 THEN
        PRINT "policy: blocco"
    ELSE
        PRINT "policy: nessuna azione"
    END IF
END SUB
