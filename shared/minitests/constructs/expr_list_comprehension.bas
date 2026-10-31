' EXPECT: map0= 2|map1= 4|filter0= 3|nested0= 11|nested3= 22
SUB Main()
    DIM doppi = [ n * 2 FOR n IN {1, 2, 3} ]
    PRINT "map0="; doppi(0)
    PRINT "map1="; doppi(1)
    DIM maggiori = [ n FOR n IN {1, 2, 3, 4} IF n > 2 ]
    PRINT "filter0="; maggiori(0)
    DIM cartesiano = [ x + y FOR x IN {10, 20} FOR y IN {1, 2} ]
    PRINT "nested0="; cartesiano(0)
    PRINT "nested3="; cartesiano(3)
END SUB
