' EXPECT: ids-ok
IMPORT CONTROLS
SCREEN 0
DIM v = 0
DIM h1 = -1
DIM h2 = -1
DIM h3 = -1
DIM h4 = -1
SUB Main()
    CHECKBOX 1, 1, "a" INTO v ID h1
    TOGGLE 2, 1, "b" INTO v ID h2
    LISTBOX 3, 1, 10, 3, "x" INTO v ID h3
    SLIDER 4, 1, 10, 0, 10, 1 INTO v ID h4
    IF h1 >= 0 AND h2 >= 0 AND h3 >= 0 AND h4 >= 0 THEN PRINT "ids-ok" ELSE PRINT "ids-no"
END SUB
