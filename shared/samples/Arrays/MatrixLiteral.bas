' FILE: Arrays/MatrixLiteral.bas
' Letterali matrice: assegnazione a matrici dichiarate con {{...}} e [[...]],
' e creazione di una matrice implicita con {{...}}.
' Verificato su 4 backend (IL, PCodeVM, VM/C, C99Native).

SUB Main()
    CLS
    PRINT "=== Matrix Literal ==="

    ' 1. Matrice dichiarata + letterale con graffe
    DIM M(3, 3)
    M = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}
    PRINT "M: "; M(0, 0); ","; M(1, 1); ","; M(2, 2)

    ' 2. Matrice dichiarata + letterale con quadre
    DIM N(3, 3)
    N = [[9, 8, 7], [6, 5, 4], [3, 2, 1]]
    PRINT "N: "; N(0, 0); ","; N(1, 1); ","; N(2, 2)

    ' 3. Somma elemento per elemento
    DIM S(3, 3)
    FOR i = 0 TO 2
        FOR j = 0 TO 2
            S(i, j) = M(i, j) + N(i, j)
        NEXT j
    NEXT i
    PRINT "S: "; S(0, 0); ","; S(1, 1); ","; S(2, 2)

    ' 4. Matrice implicita (variabile non dichiarata) da letterale con graffe
    A = {{10, 20}, {30, 40}}
    PRINT "A: "; A(0, 0); ","; A(0, 1); ","; A(1, 0); ","; A(1, 1)

    ' 5. Matrice di stringhe
    DIM G$(2, 1)
    G$ = {{"a", "b"}, {"c", "d"}, {"e", "f"}}
    PRINT "G: "; G$(0, 1); G$(1, 0); G$(2, 1)

    PRINT "=== Fine ==="
END SUB
