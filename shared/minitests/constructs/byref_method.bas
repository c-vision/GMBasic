' EXPECT: W= 12|k= 0|H= 9|j= 0|area= 324
' BYREF come parametro di un METODO DI CLASSE (2026-11-11): il call-site deve passare
' l'indirizzo della variabile del chiamante, non il valore. Prima del fix la callee scriveva
' su una copia e il chiamante vedeva il valore vecchio su PCode/VM-C (e su C99Native il
' programma non compilava affatto).
CLASS Rettangolo
    PUBLIC DIM W AS LONG
    PUBLIC DIM H AS LONG

    PUBLIC SUB Scala(BYREF k AS LONG)
        W = W * k
        H = H * k
        k = 0
    END SUB

    PUBLIC FUNCTION Area() AS LONG
        RETURN W * H
    END FUNCTION
END CLASS

SUB Main()
    DIM r AS Rettangolo
    r = NEW Rettangolo()
    r.W = 4 : r.H = 1

    DIM k AS LONG
    k = 3
    r.Scala(k)
    PRINT "W="; r.W
    PRINT "k="; k

    ' Seconda chiamata con un'altra variabile: il metodo non deve "ricordare" il primo BYREF
    DIM j AS LONG
    j = 3
    r.Scala(j)
    PRINT "H="; r.H
    PRINT "j="; j

    PRINT "area="; r.Area()
END SUB
