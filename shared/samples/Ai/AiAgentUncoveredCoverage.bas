' SURFACE: esercita la superficie del namespace (drill), non uno scenario didattico.
' FILE: AI/AiAgentUncoveredCoverage.bas
' =================================================================================================
' TEST UNITARIO: AIAGENT - FUNZIONI NON ANCORA COPERTE DAGLI ESEMPI
' =================================================================================================
' Questo test copre la funzione AIAGENT che non compariva in alcun esempio .bas:
'   STOPSERVER.
'
' COPERTURA:
' - AI.AGENTSTOPSERVER : ferma il server -> BIT.
' =================================================================================================
IMPORT AI WITH ALIAS

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "AIAGENT - Funzioni scoperte dagli esempi"
    PRINT "============================================================"
    PRINT

    ' 1. STOPSERVER
    PRINT "-- STOPSERVER --"
    DIM stopped AS BIT
    AI AGENT STOP SERVER
    PRINT "STOPSERVER() = "; stopped
    PRINT

    PRINT "Test Completato."
END SUB