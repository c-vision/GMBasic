' MiniTest END-TO-END: FIRE EVENT esegue DAVVERO la SUB gestore su TUTTE le gambe.
' EXPECT: fire-ok|2
IMPORT CONTROLS
SCREEN 0
DIM v = 0
DIM h = -1
DIM hits = 0
SUB Main()
    CHECKBOX 10, 10, "A" INTO v ID h
    ON CHANGE CONTROL h CALL OnChg
    FIRE EVENT h, "CHANGE"
    FIRE EVENT h, "CHANGE"
    IF hits = 2 THEN PRINT "fire-ok" ELSE PRINT "fire-no"
    PRINT hits
END SUB
SUB OnChg()
    hits = hits + 1
END SUB
