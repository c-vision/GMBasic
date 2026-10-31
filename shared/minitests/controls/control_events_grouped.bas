' MiniTest: eventi raggruppati `ON <EVENTO> CONTROL <id> <azione>` (CALL/STOP).
' EXPECT: grp-ok
IMPORT CONTROLS
SCREEN 0
DIM v = 0
DIM h = -1
DIM h2 = -1
SUB OnOk()
END SUB
SUB Main()
    CHECKBOX 10, 10, "A" INTO v ID h
    BUTTON 10, 50, 120, 30, "B" ID h2
    ON CLICK CONTROL h CALL OnOk()
    ON CHANGE CONTROL h STOP
    ON CLICK CONTROL h2 STOP
    IF h >= 0 AND h2 >= 0 THEN PRINT "grp-ok" ELSE PRINT "grp-no"
END SUB
