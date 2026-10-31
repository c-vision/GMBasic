' P2 AI CONTEXT — AI.CONTEXTVIEWDEBUG$(scope): vista curata grezza (audit/debug).
' EXPECT: set= ok|debug= ola
IMPORT AI

SUB Main()
    DIM agent$ = "testdbg"

    AI CONTEXT SET BUDGET agent$, 16000
    AI CONTEXT VIEW agent$, "ola"
    PRINT "set= ok"

    IF AI.CONTEXTVIEWDEBUG$(agent$) = "ola" THEN
        PRINT "debug= ola"
    ELSE
        PRINT "debug= ERR: " + AI.CONTEXTVIEWDEBUG$(agent$)
    END IF
END SUB
