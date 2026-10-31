' EXPECT: set-count= 5|set2-count= 3
SUB Main()
    DIM set_unici AS HASHSET OF DOUBLE = { x FOR x IN {1, 2, 2, 3, 4, 4, 4, 5} }
    PRINT "set-count="; set_unici.Count
    DIM set_filtro AS HASHSET OF DOUBLE = { x FOR x IN {1, 2, 3, 4, 5} IF x > 2 }
    PRINT "set2-count="; set_filtro.Count
END SUB
