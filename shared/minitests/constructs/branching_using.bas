' EXPECT: scritto= Record 1|letto= Record 1|auto-close
' Sintassi canone: samples/Files/UsingBlock.bas
SUB Main()
    USING F = OPEN("work_testusing.txt", OUTPUT)
        PRINT #F, "Record 1"
    END USING
    PRINT "scritto= Record 1"
    DIM Linea$
    USING F = OPEN("work_testusing.txt", INPUT)
        INPUT #F, Linea$
    END USING
    PRINT "letto= "; Linea$
    KILL "work_testusing.txt"
    PRINT "auto-close"
END SUB
