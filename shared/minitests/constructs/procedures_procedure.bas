' EXPECT: area= 12|scambio= 1   2|-----|default= -----
' Sintassi canone: samples/Functions/FunctionsRecursionAndByRef.bas
FUNCTION CalcolaArea(Larghezza, Altezza) AS DOUBLE
    RETURN Larghezza * Altezza
END FUNCTION
SUB Scambia(BYREF A, BYREF B)
    DIM Temp = A
    A = B
    B = Temp
END SUB
SUB StampaLinea(Lunghezza, OPTIONAL Char AS STRING = "-")
    PRINT DUP(Lunghezza, LEFT(Char, 1))
END SUB
SUB Main()
    PRINT "area="; CalcolaArea(3, 4)
    DIM X = 2, Y = 1
    Scambia(X, Y)
    PRINT "scambio="; X; " "; Y
    StampaLinea(5)
    PRINT "default= -----"
END SUB
