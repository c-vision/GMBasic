' MiniTest: AI DECIDE in forma BATCH (JEV/SystemOne) — N domande, un solo FROM, END DECIDE.
' v1: desugar in N decisioni sequenziali (stessa semantica della forma singola); la fusione
' prefill/KV (DECISION_GROUP) è l'ottimizzazione successiva. Verifica su 4 gambe.
' EXPECT: batch-fraud=false| 0.5|batch-route=technical| 1|batch-sev=0| 0.5|batch-mock=false| 1
IMPORT AI

SUB Main()
    DIM fraud AS DECISION
    DIM route AS DECISION
    DIM sev AS DECISION
    AI DECIDE
        fraud AS NOUL
        route AS CHOICE ("billing", "technical", "sales")
        sev AS SCORE 0 TO 5
    FROM "technical issue"
    END DECIDE
    PRINT "batch-fraud="; fraud.VALUE; "|"; fraud.PROBABILITY
    PRINT "batch-route="; route.VALUE; "|"; route.PROBABILITY
    PRINT "batch-sev="; sev.VALUE; "|"; sev.PROBABILITY

    DIM m AS DECISION
    AI DECIDE
        m AS NOUL
    FROM "x"
    USING "mock"
    END DECIDE
    PRINT "batch-mock="; m.VALUE; "|"; m.PROBABILITY
END SUB
