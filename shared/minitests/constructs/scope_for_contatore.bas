' EXPECT: dentro I= 1|dentro I= 2|dentro I= 3|dopo I= 50|temp dopo= 0
' CONTATORE FOR e scope di blocco (spec Variables-Scope §6.1): «la variabile contatore (I) è
' sempre locale al blocco del FOR» — il ciclo usa una I del BLOCCO e al NEXT quella esterna torna
' al suo valore. Una variabile dichiarata nel corpo del ciclo (Temp) vive solo nel blocco: letta
' dopo vale il default del tipo (0).
SUB ForShadow()
    DIM I AS LONG
    I = 50
    FOR I = 1 TO 3
        DIM Temp AS LONG
        Temp = I * 10
        PRINT "dentro I="; I
    NEXT I
    PRINT "dopo I="; I
    PRINT "temp dopo="; Temp
END SUB

SUB Main()
    ForShadow()
END SUB
