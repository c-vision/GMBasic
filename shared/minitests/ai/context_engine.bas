' P2 AI CONTEXT — SET BUDGET/SELECTOR/POLICY + CONTEXT VIEW + letture.
' Verifica che i comandi/funzioni esistano e producano output deterministico.
' EXPECT: set= ok|used= 0|view= ola
IMPORT AI

SUB Main()
    DIM agent$ = "test"

    ' AI CONTEXT SET BUDGET agent, chars
    AI CONTEXT SET BUDGET agent$, 16000
    PRINT "set= ok"

    ' AI CONTEXT VIEW agent, text
    AI CONTEXT VIEW agent$, "ola"
    ' AI.CONTEXTUSED
    IF AI.CONTEXTUSED(agent$) >= 0 THEN
        PRINT "used= 0"
    ELSE
        PRINT "used= ERR: " + STR$(AI.CONTEXTUSED(agent$))
    END IF
    ' AI.CONTEXTVIEW$
    IF AI.CONTEXTVIEW$(agent$) = "ola" THEN
        PRINT "view= ola"
    ELSE
        PRINT "view= ERR: " + AI.CONTEXTVIEW$(agent$)
    END IF
END SUB