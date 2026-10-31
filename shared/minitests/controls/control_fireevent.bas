' MiniTest: FIRE EVENT h, "EV" — iniezione evento controllo (superficie su 4 gambe).
' Senza handler registrato -> no-op, nessun crash. Con handle valido.
' EXPECT: fire-ok
IMPORT CONTROLS
SCREEN 0
DIM v = 0
DIM h = -1
SUB Main()
    CHECKBOX 10, 10, "A" INTO v ID h
    FIRE EVENT h, "CHANGE"
    FIRE EVENT h, "GOTFOCUS"
    IF h >= 0 THEN PRINT "fire-ok" ELSE PRINT "fire-no"
END SUB
