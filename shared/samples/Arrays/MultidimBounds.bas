' ============================================================================
' MultidimBounds.bas — matrici e tensori con lower bound espliciti
' Categoria: Arrays
' Mostra DIM a(1 TO n, ...) da 1 a 4 dimensioni: indici utente, LBOUND/UBOUND
' per asse, lower misti e negativi, letterali annidati, FOR EACH e un campo 3-D.
' Le dimensioni possono venire da variabili; il lower bound e' una costante.
' ============================================================================

TYPE Sample
    Value AS DOUBLE
    Label AS STRING
END TYPE

SUB Main()
    PRINT "=== MultidimBounds ==="

    ' ── 1-D: una serie con indici 1..7 ──────────────────────────────────────
    DIM temp(1 TO 7) AS DOUBLE
    DIM i AS LONG
    FOR i = 1 TO UBOUND(temp, 1)
        temp(i) = 2 * i + 2
    NEXT i
    DIM somma AS DOUBLE
    DIM minimo AS DOUBLE = temp(1)
    DIM massimo AS DOUBLE = temp(1)
    FOR EACH v IN temp
        somma = somma + v
        IF v < minimo THEN minimo = v
        IF v > massimo THEN massimo = v
    NEXT
    PRINT "serie: lb="; LBOUND(temp, 1); " ub="; UBOUND(temp, 1)
    PRINT "       somma="; somma; " min="; minimo; " max="; massimo

    ' ── 2-D: griglia con dimensioni calcolate a runtime ─────────────────────
    DIM righe AS LONG = 3
    DIM colonne AS LONG = 4
    DIM griglia(1 TO righe, 1 TO colonne) AS DOUBLE
    FOR i = 1 TO righe
        FOR j = 1 TO colonne
            griglia(i, j) = i * 10 + j
        NEXT j
    NEXT i
    PRINT "griglia: lb="; LBOUND(griglia, 1); ","; LBOUND(griglia, 2);
    PRINT " ub="; UBOUND(griglia, 1); ","; UBOUND(griglia, 2)
    PRINT "         angoli="; griglia(1, 1); ","; griglia(righe, colonne)
    DIM riga AS DOUBLE
    FOR i = 1 TO righe
        riga = 0
        FOR j = 1 TO colonne
            riga = riga + griglia(i, j)
        NEXT j
        PRINT "         riga "; i; " somma="; riga
    NEXT i

    ' ── 2-D con lower misti: 0..2 e 1..3 ────────────────────────────────────
    DIM finestra(0 TO 2, 1 TO 3) AS DOUBLE
    FOR i = 0 TO 2
        FOR j = 1 TO 3
            finestra(i, j) = i * 3 + j
        NEXT j
    NEXT i
    PRINT "finestra: lb="; LBOUND(finestra, 1); ","; LBOUND(finestra, 2);
    PRINT " ub="; UBOUND(finestra, 1); ","; UBOUND(finestra, 2)
    PRINT "          inizi="; finestra(0, 1); " fine="; finestra(2, 3)

    ' ── 3-D: un campo scalare ───────────────────────────────────────────────
    DIM nx AS LONG = 3
    DIM ny AS LONG = 4
    DIM nz AS LONG = 2
    DIM campo(1 TO nx, 1 TO ny, 1 TO nz) AS DOUBLE
    DIM totale AS DOUBLE
    FOR i = 1 TO nx
        FOR j = 1 TO ny
            FOR k = 1 TO nz
                campo(i, j, k) = i * j + k
                totale = totale + campo(i, j, k)
            NEXT k
        NEXT j
    NEXT i
    PRINT "campo 3-D: ub="; UBOUND(campo, 1); ","; UBOUND(campo, 2); ","; UBOUND(campo, 3)
    PRINT "           totale="; totale; " [1,1,1]="; campo(1, 1, 1); " [3,4,2]="; campo(3, 4, 2)

    ' ── 4-D: un ipercubo indicizzato 1..2 su ogni asse ──────────────────────
    DIM hyper(1 TO 2, 1 TO 2, 1 TO 2, 1 TO 2) AS DOUBLE
    DIM somma4 AS DOUBLE
    FOR a = 1 TO 2
        FOR b = 1 TO 2
            FOR c = 1 TO 2
                FOR d = 1 TO 2
                    hyper(a, b, c, d) = a * 8 + b * 4 + c * 2 + d
                    somma4 = somma4 + hyper(a, b, c, d)
                NEXT d
            NEXT c
        NEXT b
    NEXT a
    PRINT "ipercubo: ub4="; UBOUND(hyper, 4); " primo="; hyper(1, 1, 1, 1); " ultimo="; hyper(2, 2, 2, 2)
    PRINT "          totale="; somma4

    ' ── Letterale 3-D: un kernel 2x2x2 ──────────────────────────────────────
    DIM kernel(1 TO 2, 1 TO 2, 1 TO 2) AS DOUBLE = {{{1, 2}, {3, 4}}, {{5, 6}, {7, 8}}}
    PRINT "kernel: [1,1,1]="; kernel(1, 1, 1); " [2,2,2]="; kernel(2, 2, 2)

    ' ── Array di UDT con lower bound ────────────────────────────────────────
    DIM campioni(1 TO 2, 1 TO 2) AS Sample
    campioni(1, 1).Value = 3.5 : campioni(1, 1).Label = "a"
    campioni(2, 2).Value = 7   : campioni(2, 2).Label = "d"
    PRINT "campioni: "; campioni(1, 1).Value; "/"; campioni(1, 1).Label;
    PRINT " "; campioni(2, 2).Value; "/"; campioni(2, 2).Label;
    PRINT " lb="; LBOUND(campioni, 1); " ub="; UBOUND(campioni, 2)

    ' ── Lower negativi: una matrice di offset -1..1 ──────────────────────────
    DIM offset(-1 TO 1, -1 TO 1) AS DOUBLE
    offset(-1, -1) = -1 : offset(0, 0) = 0 : offset(1, 1) = 1
    PRINT "offset: lb="; LBOUND(offset, 1); ","; LBOUND(offset, 2);
    PRINT " ub="; UBOUND(offset, 1); ","; UBOUND(offset, 2)
    PRINT "        diag="; offset(-1, -1); ","; offset(0, 0); ","; offset(1, 1)
END SUB
