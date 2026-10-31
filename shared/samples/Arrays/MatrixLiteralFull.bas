' FILE: Arrays/MatrixLiteralFull.bas
' Esercita: letterali matrice {{...}} e [[...]], matrice implicita, matrice di stringhe,
' scrittura con quadre su array tipizzati, UBOUND su 1D/2D, iterazione e aritmetica.

SUB Main()
    CLS
    PRINT "=== Matrix Literal Full ==="

    ' 1. Matrice dichiarata da graffe
    DIM A(2, 2)
    A = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}

    ' 2. Matrice dichiarata da quadre
    DIM B(2, 2)
    B = [[9, 8, 7], [6, 5, 4], [3, 2, 1]]

    ' 3. Somma elemento per elemento (scrittura con quadre, lettura con tonde)
    DIM C(2, 2)
    FOR i = 0 TO UBOUND(C, 1)
        FOR j = 0 TO UBOUND(C, 2)
            C[i, j] = A(i, j) + B(i, j)
        NEXT j
    NEXT i
    PRINT "C(0,0)="; C(0, 0); " C(2,2)="; C(2, 2)

    ' 4. Traccia (somma diagonale)
    DIM tr = 0
    FOR i = 0 TO UBOUND(A, 1)
        tr = tr + A(i, i)
    NEXT i
    PRINT "trace(A)="; tr

    ' 5. Matrice implicita + matrice di stringhe
    M = {{10, 20}, {30, 40}}
    PRINT "M="; M(0, 0); ","; M(0, 1); ","; M(1, 0); ","; M(1, 1)

    DIM G$(2, 1)
    G$ = {{"a", "b"}, {"c", "d"}, {"e", "f"}}
    PRINT "G="; G$(0, 0); G$(1, 1); G$(2, 0)

    ' 6. Array tipizzato: scrittura [] + UBOUND
    DIM L(4) AS LONG
    FOR i = 0 TO 4 : L[i] = i * i : NEXT i
    PRINT "L="; L[0]; ","; L[1]; ","; L[2]; ","; L[3]; ","; L[4]
    PRINT "ub(L)="; UBOUND(L)

    PRINT "=== Fine ==="
END SUB
