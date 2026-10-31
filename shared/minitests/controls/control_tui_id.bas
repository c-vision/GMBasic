' EXPECT: tui-id-ok
IMPORT CONTROLS
SCREEN 0
DIM v = 0
DIM h = -1
SUB Main()
    CHECKBOX 1, 1, "x" INTO v ID h
    IF h >= 0 THEN PRINT "tui-id-ok" ELSE PRINT "tui-id-no"
END SUB
