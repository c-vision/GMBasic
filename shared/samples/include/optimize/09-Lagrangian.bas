REM FILE: 09-Lagrangian.bas — pacchetto `lagrangian` (FFI): LAGRANGIANSUBGRADIENT.
REM Scenario: pianificazione di produzione multi-risorsa. 4 prodotti, 2 risorse, lotti interi
REM 0..3. Si rilassano i due vincoli di risorsa con moltiplicatori lambda >= 0 e si risolve
REM esattamente il subproblema per enumerazione (256 combinazioni) DENTRO la callback.
REM L'ottimo vero si calcola per forza bruta nello stesso sample: il confronto mostra che il
REM rilassamento produce un LOWER BOUND valido (LB <= OPT) e quanto stringe con piu' iterazioni.
IMPORT OPTIMIZE

' Dati del problema (globali: li legge la callback).
DIM PPrezzo(3) AS DOUBLE      ' costo unitario
DIM R1(3) AS DOUBLE           ' ore risorsa 1 per unita'
DIM R2(3) AS DOUBLE           ' ore risorsa 2 per unita'
DIM BR1 AS DOUBLE
DIM BR2 AS DOUBLE
DIM NCall AS LONG

' Callback del subproblema: min_x  p'x + lam1*(b1 - r1'x) + lam2*(b2 - r2'x), x in {0..3}^4.
' Ritorna il vettore [valore, x0, x1, x2, x3] (forma "ritorno ARRAY" del pacchetto).
FUNCTION Sub(lam() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM r(4) AS DOUBLE
    NCall = NCall + 1
    best = 1000000000.0
    FOR a = 0 TO 3
        FOR b = 0 TO 3
            FOR c = 0 TO 3
                FOR e = 0 TO 3
                    v = PPrezzo(0)*a + PPrezzo(1)*b + PPrezzo(2)*c + PPrezzo(3)*e
                    v = v + lam(0) * (BR1 - R1(0)*a - R1(1)*b - R1(2)*c - R1(3)*e)
                    v = v + lam(1) * (BR2 - R2(0)*a - R2(1)*b - R2(2)*c - R2(3)*e)
                    IF v < best THEN
                        best = v : ba = a : bb = b : bc = c : be = e
                    END IF
                NEXT e
            NEXT c
        NEXT b
    NEXT a
    r(0) = best : r(1) = ba : r(2) = bb : r(3) = bc : r(4) = be
    RETURN r
END FUNCTION

' Ottimo esatto per forza bruta (solo per il confronto nel sample).
FUNCTION OttimoEsatto() AS DOUBLE
    best = 1000000000.0
    FOR a = 0 TO 3
        FOR b = 0 TO 3
            FOR c = 0 TO 3
                FOR e = 0 TO 3
                    s1 = R1(0)*a + R1(1)*b + R1(2)*c + R1(3)*e
                    s2 = R2(0)*a + R2(1)*b + R2(2)*c + R2(3)*e
                    IF s1 >= BR1 AND s2 >= BR2 THEN
                        v = PPrezzo(0)*a + PPrezzo(1)*b + PPrezzo(2)*c + PPrezzo(3)*e
                        IF v < best THEN best = v
                    END IF
                NEXT e
            NEXT c
        NEXT b
    NEXT a
    RETURN best
END FUNCTION

SUB Main()
    PPrezzo(0)=5 : PPrezzo(1)=4 : PPrezzo(2)=3 : PPrezzo(3)=7
    R1(0)=2 : R1(1)=3 : R1(2)=1 : R1(3)=4
    R2(0)=1 : R2(1)=1 : R2(2)=3 : R2(3)=1
    BR1 = 6
    BR2 = 5

    ' Matrice dei vincoli A (m x nx row-major) e termini b.
    DIM A(7) AS DOUBLE
    A(0)=2 : A(1)=3 : A(2)=1 : A(3)=4
    A(4)=1 : A(5)=1 : A(6)=3 : A(7)=1
    DIM b(1) AS DOUBLE
    b(0)=6 : b(1)=5

    opt = OttimoEsatto()
    PRINT "ottimo="; FORMAT(opt, "#.00")

    ' ── maxit 200: bound stretto ────────────────────────────────────────────────────
    NCall = 0
    DIM out(6) AS DOUBLE          ' 1 + m(2) + nx(4)
    lb = OPTIMIZE.LAGRANGIANSUBGRADIENT(Sub, A, b, 2, 4, 200, out)
    PRINT "|lb200="; FORMAT(lb, "#.0000"); ":"; (lb <= opt); (lb > 0); ":"; (out(0) = lb)
    PRINT "|gap="; FORMAT(opt - lb, "#.0000"); "|call="; (NCall > 0)
    PRINT "|lambda="; FORMAT(out(1), "#.000"); ","; FORMAT(out(2), "#.000"); ":"; (out(1) >= 0); (out(2) >= 0)
    PRINT "|x="; FORMAT(out(3), "#"); FORMAT(out(4), "#"); FORMAT(out(5), "#"); FORMAT(out(6), "#")

    ' ── maxit 10: meno iterazioni -> bound non migliore ─────────────────────────────
    DIM out2(6) AS DOUBLE
    lb10 = OPTIMIZE.LAGRANGIANSUBGRADIENT(Sub, A, b, 2, 4, 10, out2)
    PRINT "|lb10="; FORMAT(lb10, "#.0000"); ":"; (lb10 <= opt); ":"; (lb10 <= lb + 0.000001)
    PRINT ""
END SUB
