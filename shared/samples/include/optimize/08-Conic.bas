REM FILE: 08-Conic.bas — pacchetto `conic` (FFI): usa TUTTE le 7 funzioni EXPCONE*
REM (EXPCONEBARRIER, EXPCONEGRAD, EXPCONEHESS, EXPCONEDUALIN, EXPCONEDUALPOINT,
REM EXPCONENTMETRICS, EXPCONESCALING).
REM Scenario: validazione numerica di un generatore di cono (PEXP/PPOW/RPOW) come lo usa un
REM solver di punto interno — barriera, gradiente e Hessiana confrontati con differenze finite
REM centrali, certificato duale, metriche NT e scalatura. Basta la sola aritmetica: nessuna
REM dipendenza da BLAS, risultato deterministico sui 4 backend.
IMPORT OPTIMIZE

' Parametri del cono corrente (globali: li leggono le funzioni di supporto).
DIM gKind AS LONG
DIM gAlpha AS DOUBLE

FUNCTION Barriera(x() AS DOUBLE) AS DOUBLE
    DIM f(0) AS DOUBLE
    OPTIMIZE.EXPCONEBARRIER(gKind, gAlpha, x, f)
    RETURN f(0)
END FUNCTION

' Errore massimo fra gradiente analitico e differenze finite centrali della barriera.
FUNCTION ErroreGrad(x() AS DOUBLE) AS DOUBLE
    DIM g(2) AS DOUBLE
    DIM y(2) AS DOUBLE
    OPTIMIZE.EXPCONEGRAD(gKind, gAlpha, x, g)
    emax = 0
    passo = 0.00001
    FOR i = 0 TO 2
        FOR j = 0 TO 2 : y(j) = x(j) : NEXT j
        y(i) = x(i) + passo
        fp = Barriera(y)
        y(i) = x(i) - passo
        fm = Barriera(y)
        d = ABS((fp - fm) / (2 * passo) - g(i))
        IF d > emax THEN emax = d
    NEXT i
    RETURN emax
END FUNCTION

' Errore massimo fra Hessiana analitica e differenze finite centrali del gradiente.
FUNCTION ErroreHess(x() AS DOUBLE) AS DOUBLE
    DIM H(8) AS DOUBLE
    DIM y(2) AS DOUBLE
    DIM gp(2) AS DOUBLE
    DIM gm(2) AS DOUBLE
    OPTIMIZE.EXPCONEHESS(gKind, gAlpha, x, H)
    emax = 0
    passo = 0.00001
    FOR i = 0 TO 2
        FOR j = 0 TO 2 : y(j) = x(j) : NEXT j
        y(i) = x(i) + passo
        OPTIMIZE.EXPCONEGRAD(gKind, gAlpha, y, gp)
        y(i) = x(i) - passo
        OPTIMIZE.EXPCONEGRAD(gKind, gAlpha, y, gm)
        FOR j = 0 TO 2
            d = ABS((gp(j) - gm(j)) / (2 * passo) - H(i * 3 + j))
            IF d > emax THEN emax = d
        NEXT j
    NEXT i
    RETURN emax
END FUNCTION

' Hessiana definita positiva: H11 > 0, minore 2x2 > 0, determinante > 0 (Sylvester).
FUNCTION Convessa(x() AS DOUBLE) AS LONG
    DIM H(8) AS DOUBLE
    OPTIMIZE.EXPCONEHESS(gKind, gAlpha, x, H)
    IF H(0) <= 0 THEN RETURN 0
    m2 = H(0) * H(4) - H(1) * H(3)
    IF m2 <= 0 THEN RETURN 0
    det = H(0) * (H(4) * H(8) - H(5) * H(7))
    det = det - H(1) * (H(3) * H(8) - H(5) * H(6))
    det = det + H(2) * (H(3) * H(7) - H(4) * H(6))
    IF det <= 0 THEN RETURN 0
    RETURN 1
END FUNCTION

' Valida il cono corrente sul punto x(): riga di certificato.
SUB Certificato(nome AS STRING, x() AS DOUBLE)
    DIM D(2) AS DOUBLE
    DIM M(6) AS DOUBLE
    PRINT nome; "="; FORMAT(Barriera(x), "#.0000");
    PRINT ":"; (ErroreGrad(x) < 0.0001); (ErroreHess(x) < 0.001); (Convessa(x) <> 0)

    ' Certificato duale: w = punto di scaling duale del barrier (stesso cono).
    ' Scegliamo s interno a K* e verifichiamo la membership + il vettore w.
    IF gKind = 0 THEN
        D(0)=2 : D(1)=1 : D(2)=-1
    ELSE
        D(0)=2 : D(1)=2 : D(2)=-1
    END IF
    PRINT "|dual="; OPTIMIZE.EXPCONEDUALIN(gKind, gAlpha, D); ":";
    PRINT OPTIMIZE.EXPCONEDUALPOINT(gKind, gAlpha, D, M); ":"; (M(0) <> 0)

    ' Metriche NT: GAP (= z's), CONDZ, CONDS, REL, MZ, MS, WN — la gap deve essere >= 0.
    PRINT "|met="; OPTIMIZE.EXPCONENTMETRICS(gKind, gAlpha, x, D, M); ":";
    PRINT FORMAT(M(0), "#.000"); ":"; (M(0) >= 0); (M(6) > 0)
END SUB

SUB Main()
    DIM p(2) AS DOUBLE

    ' ── PEXP: t >= u*e^(v/u) — punto (2,1,0), barriera vicino al bordo cresce ────────
    gKind = 0
    gAlpha = 0.5
    p(0)=2 : p(1)=1 : p(2)=0
    Certificato("pexp", p)
    DIM q(2) AS DOUBLE
    q(0)=1.2 : q(1)=1 : q(2)=0
    PRINT "|barriera="; (Barriera(q) > Barriera(p))

    ' ── PPOW e RPOW con alpha: il parametro cambia il generatore ─────────────────────
    gKind = 1
    gAlpha = 0.5
    p(0)=2 : p(1)=2 : p(2)=1
    Certificato("ppow", p)
    ' alpha cambia il generatore: punto ASIMMETRICO (t=3, u=2, v=1) — con t=u il
    ' generatore e' alpha-invariante (t^2a * u^(2-2a) = t^2), quindi non prova nulla.
    DIM pa(2) AS DOUBLE
    pa(0)=3 : pa(1)=2 : pa(2)=1
    gAlpha = 0.3
    b03 = Barriera(pa)
    PRINT "|alpha="; FORMAT(b03, "#.0000"); ":";
    gAlpha = 0.7
    b07 = Barriera(pa)
    PRINT FORMAT(b07, "#.0000"); ":"; (b03 <> b07)

    gKind = 2
    gAlpha = 0.5
    Certificato("rpow", p)

    ' ── Scalatura NT: Theta (9) | Thin (9) | rt (9) | scale, per un passo di Newton ───
    DIM D(2) AS DOUBLE
    DIM SC(27) AS DOUBLE
    D(0)=2 : D(1)=2 : D(2)=-1
    PRINT "|scl="; OPTIMIZE.EXPCONESCALING(1, 0.5, p, D, 1.0, SC); ":";
    PRINT FORMAT(SC(27), "#.000");
    ' Theta = inv(Thin) e' simmetrica a meno dell'errore numerico dell'inversa; la scala
    ' riportata in SC(27) e' la gap primale-duale (stessa delle metriche NT).
    PRINT ":"; (ABS(SC(1) - SC(3)) < 0.000001); (ABS(SC(2) - SC(6)) < 0.000001);
    PRINT (ABS(SC(5) - SC(7)) < 0.000001); (SC(0) <> 0); (SC(4) <> 0); (SC(8) <> 0)
    DIM M2(6) AS DOUBLE
    OPTIMIZE.EXPCONENTMETRICS(1, 0.5, p, D, M2)
    PRINT "|gap="; (ABS(M2(0) - SC(27)) < 0.000001)

    ' ── Punto fuori dal cono: le funzioni di cono rifiutano (-1) ─────────────────────
    DIM bad(2) AS DOUBLE
    DIM f(0) AS DOUBLE
    DIM g(2) AS DOUBLE
    bad(0)=1 : bad(1)=1 : bad(2)=5      ' v^2 > 4*t*u -> fuori
    PRINT "|fuori="; OPTIMIZE.EXPCONEBARRIER(1, 0.5, bad, f);
    PRINT OPTIMIZE.EXPCONEGRAD(1, 0.5, bad, g);
    PRINT OPTIMIZE.EXPCONEHESS(1, 0.5, bad, g)
    PRINT ""
END SUB
