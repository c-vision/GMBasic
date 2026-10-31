' MiniTest: ON <EV> CONTROL <h> GOSUB/GOTO <SUB> (azioni GOSUB e GOTO, tutte le gambe).
' EXPECT: lbl-ok
IMPORT CONTROLS
SCREEN 0
DIM v = 0
DIM h = -1
DIM fired = 0
SUB Main()
    CHECKBOX 10, 10, "A" INTO v ID h
    ON CHANGE CONTROL h GOSUB OnChg
    ON GOTFOCUS CONTROL h GOTO OnGot
    fired = 1
    IF fired = 1 THEN PRINT "lbl-ok" ELSE PRINT "lbl-no"
END SUB
SUB OnChg()
    fired = 2
END SUB
SUB OnGot()
    fired = 3
END SUB
