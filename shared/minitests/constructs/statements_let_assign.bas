' EXPECT: let= 42|assign= 43
SUB Main()
    LET x = 42
    PRINT "let="; x
    DIM y = 42
    y = y + 1
    PRINT "assign="; y
END SUB
