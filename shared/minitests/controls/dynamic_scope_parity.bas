' DYNAMIC (scope dinamico, Guide/Variables-Scope-Spec §2.5): cascata + shadowing + ripristino.
' EXPECT: cascata=uno|B=due|A=uno
SUB A()
    DYNAMIC x = "uno"
    CALL B()
    PRINT "A="; x
END SUB
SUB B()
    DYNAMIC x = "due"
    PRINT "B="; x
END SUB
SUB Chiamante()
    DYNAMIC y = "uno"
    CALL Legge()
END SUB
SUB Legge()
    PRINT "cascata="; y
END SUB
CALL Chiamante()
CALL A()
