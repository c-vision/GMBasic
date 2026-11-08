' Array di UDT i cui elementi hanno un campo ARRAY: accesso e assegnazione per elemento.
' EXPECT: a= 5|a4= 9|b= 7|a0= 0
TYPE Arr4
    A(4) AS LONG
    B AS DOUBLE
END TYPE
SUB Main()
    DIM v(1) AS Arr4
    v(1).A(0) = 5
    v(1).A(4) = 9
    v(1).B = 7
    PRINT "a="; v(1).A(0)
    PRINT "a4="; v(1).A(4)
    PRINT "b="; v(1).B
    PRINT "a0="; v(0).A(0)
END SUB
