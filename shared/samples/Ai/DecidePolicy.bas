' AI DECIDE + POLICY — la decisione produce EVIDENZA; la soglia resta nel programma.
' Eseguibile senza modello: provider "rules" deterministico.
IMPORT AI

SUB Main()
    DIM fraud AS DECISION
    AI DECIDE
        fraud AS NOUL
    FROM "Il cliente ha detto sì: è una frode?"
    USING "rules"
    END DECIDE

    PRINT "evidenza: "; fraud.VALUE; " p="; fraud.PROBABILITY
    ' Policy applicativa: nessun side effect dentro il provider.
    IF fraud.VALUE = "true" AND fraud.PROBABILITY >= 0.8 THEN
        PRINT "policy: blocco"
    ELSE
        PRINT "policy: nessuna azione"
    END IF
END SUB
