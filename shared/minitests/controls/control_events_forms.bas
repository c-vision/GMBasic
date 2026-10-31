' MiniTest: forme azione evento controllo (ONCLICK attaccato, GOSUB, STOP) + ID.
' EXPECT: forms-ok
IMPORT CONTROLS
SCREEN 0
DIM v = 0
DIM v2 = 0
DIM h = -1
DIM h2 = -1
SUB OnOk()
END SUB
SUB OnChg()
END SUB
SUB Main()
    BUTTON 10, 10, 120, 30, "OK" ID h ONCLICK OnOk()
    CHECKBOX 10, 50, "A" INTO v ID h2 ONCHANGE GOSUB OnChg
    CHECKBOX 10, 80, "B" INTO v2 ONCLICK STOP
    IF h >= 0 AND h2 >= 0 THEN PRINT "forms-ok" ELSE PRINT "forms-no"
END SUB
