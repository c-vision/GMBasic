' MiniTest: ID handle del controllo (CHECK + BUTTON) su 4 backend.
' EXPECT: id-ok
IMPORT CONTROLS
SCREEN 0
DIM v = 0
DIM h1 = -1
DIM h2 = -1
SUB Main()
    CHECKBOX 10, 10, "A" INTO v ID h1
    BUTTON 10, 40, 100, 30, "B" ID h2
    IF h1 >= 0 AND h2 >= 0 THEN PRINT "id-ok" ELSE PRINT "id-no"
END SUB
