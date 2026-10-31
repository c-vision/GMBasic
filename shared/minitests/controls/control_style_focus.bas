' MiniTest: clausole FOCUS e STYLE sui controlli CONTROLS.
' EXPECT: fs-ok
IMPORT CONTROLS
SCREEN 0
DIM v = 0
DIM h = -1
DIM vis = 0
SUB Main()
    CHECKBOX 1, 1, "A" INTO v ID h NAME "k1" VISIBLE 1 FOCUS STYLE "fg=#FF0000 uppercase"
    CONTROL GET "k1", "visible" INTO vis
    IF h >= 0 AND vis = 1 THEN PRINT "fs-ok" ELSE PRINT "fs-no"
END SUB
