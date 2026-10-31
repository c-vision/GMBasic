' EXPECT: o.I.X= 3.5|o.Y= 7|t.A.B.C= 9
' Bug UDT-Campo-Annidato-Scrittura-Encadenata-Non-Persiste: la scrittura/lettura di un campo
' UDT annidato (o.I.X, dove Outer.I AS Inner) deve persistere e leggere il valore corretto su
' tutti e 5 i backend — copre sia 2 livelli (o.I.X) sia 3 livelli (t.A.B.C).
TYPE Inner
    X AS DOUBLE
END TYPE
TYPE Outer
    I AS Inner
    Y AS LONG
END TYPE
TYPE Level3
    C AS DOUBLE
END TYPE
TYPE Level2
    B AS Level3
END TYPE
TYPE Level1
    A AS Level2
END TYPE
SUB Main()
    DIM o AS Outer
    o.I.X = 3.5
    PRINT "o.I.X="; o.I.X
    o.Y = 7
    PRINT "o.Y="; o.Y
    DIM t AS Level1
    t.A.B.C = 9
    PRINT "t.A.B.C="; t.A.B.C
END SUB
