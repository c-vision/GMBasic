' FILE: Branches/BranchesSwitchExpr.bas
' SWITCH expression

SUB Main()
    PRINT "SWITCH expression demo"

    DIM dayName = SWITCH(3) { CASE 1: "Lun", CASE 2: "Mar", CASE 3: "Mer", CASE 4: "Gio", CASE 5: "Ven", CASE 6: "Sab", CASE 7: "Dom", ELSE: "?" }
    PRINT "Day 3 = "; dayName

    DIM unknown = SWITCH(42) { CASE 1: "Uno", CASE 2: "Due", ELSE: "Altro" }
    PRINT "42 = "; unknown

    PRINT ".bas = "; SWITCH(".bas") { CASE ".bas": "BASIC", CASE ".cs": "C#", CASE ".py": "Python", ELSE: "Sconosciuto" }

    PRINT "Score 85 = "; SWITCH(INT(85 / 10)) { CASE 10: "A+", CASE 9: "A", CASE 8: "B", CASE 7: "C", CASE 6: "D", ELSE: "F" }

    PRINT "OK. SWITCH expression test completato."
END SUB
