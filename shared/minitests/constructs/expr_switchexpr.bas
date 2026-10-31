' EXPECT: day3=Mer|day42=Altro|score=B
SUB Main()
    DIM dayName = SWITCH(3) { CASE 1: "Lun", CASE 2: "Mar", CASE 3: "Mer", CASE 4: "Gio", ELSE: "?" }
    PRINT "day3="; dayName
    DIM unknown = SWITCH(42) { CASE 1: "Uno", ELSE: "Altro" }
    PRINT "day42="; unknown
    PRINT "score="; SWITCH(INT(85 / 10)) { CASE 10: "A+", CASE 9: "A", CASE 8: "B", ELSE: "F" }
END SUB
