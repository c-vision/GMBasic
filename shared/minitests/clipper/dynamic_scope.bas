' MiniTest DYNAMIC — scope dinamico: cascata, modifica dal callee, shadowing, ripristino.
' La visibilità di una variabile DYNAMIC dipende dalla CATENA DI CHIAMATE a runtime:
' una procedura chiamata vede (e modifica) la DYNAMIC dichiarata dal chiamante.
' EXPECT: a= 11|a2= 11|b2= 100
SUB Mod()
    a = a + 1
END SUB

SUB Shadow()
    DYNAMIC a = 99
    a = a + 1
    b = a
END SUB

SUB Main()
    DYNAMIC a = 10
    DYNAMIC b = 0
    Mod()
    PRINT "a="; a
    Shadow()
    PRINT "a2="; a
    PRINT "b2="; b
END SUB
