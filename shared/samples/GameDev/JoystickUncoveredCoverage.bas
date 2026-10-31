' FILE: GameDev/JoystickUncoveredCoverage.bas
' =================================================================================================
' TEST UNITARIO: JOY - FUNZIONI NON ANCORA COPERTE DAGLI ESEMPI
' =================================================================================================
' Questo test copre le funzioni/costanti JOY che non comparivano in alcun esempio .bas:
'   JOYPRESENT, %JOYBUTTON2.
'
' COPERTURA:
' - AVAILABLE(port)   : joystick presente? -> BIT.
' - %JOYBUTTON2        : bit pulsante 2 (valore 32), sempre disponibile senza IMPORT.
' =================================================================================================
IMPORT JOY

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "JOY - Funzioni scoperte dagli esempi"
    PRINT "============================================================"
    PRINT

    ' 1. JOYPRESENT
    PRINT "-- JOYPRESENT --"
    IF AVAILABLE(0) THEN
        PRINT "Joystick 0 presente."
    ELSE
        PRINT "Joystick 0 assente."
    END IF
    PRINT

    ' 2. %JOYBUTTON2 (bit 32)
    PRINT "-- %JOYBUTTON2 --"
    PRINT "%JOYBUTTON2 = "; %JOYBUTTON2
    ' Legge lo stato del joystick 0 e controlla il pulsante 2
    DIM stato% = MASK(0)
    IF stato% AND %JOYBUTTON2 THEN
        PRINT "Pulsante 2 premuto."
    ELSE
        PRINT "Pulsante 2 non premuto."
    END IF
    PRINT

    PRINT "Test Completato."
END SUB
