' EXPECT: ctrl-ok
IMPORT CONTROLS
SCREEN 0
DIM v = 0
DIM h = -1
DIM n = 0
DIM vis = 1
SUB Main()
    CHECKBOX 1, 1, "A" INTO v ID h NAME "k1" VISIBLE 1
    CONTROL SET "k1", "value", 1
    CONTROL GET "k1", "value" INTO n
    CONTROL SET "k1", "visible", 0
    CONTROL GET "k1", "visible" INTO vis
    IF n = 1 AND vis = 0 THEN PRINT "ctrl-ok" ELSE PRINT "ctrl-no"
END SUB
