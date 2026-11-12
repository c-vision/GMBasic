REM FILE: 07-LocalSearch.bas — pacchetto `localsearch` (FFI): usa TUTTE le 5 funzioni di
REM OPTIMIZE.LS* (LSSOLVE, LSSOLVECFG, LSNEAREST, LSCHEAPEST, LSNOWMS).
REM Scenario: giro del commesso viaggiatore su 8 città su griglia intera. Le distanze sono
REM QUADRATI esatti (aritmetica intera) → obiettivo esatto, nessuna dipendenza dal FP: il
REM risultato è identico sui 4 backend. Output deterministico.
IMPORT OPTIMIZE

' Distanza al quadrato fra le città (matrice 8x8 row-major, globale: la legge la callback).
DIM Gd(63) AS DOUBLE

' Obiettivo della ricerca locale: somma dei quadrati delle distanze lungo il giro (ciclo).
FUNCTION GiroCosto(tour() AS DOUBLE) AS DOUBLE
    s = 0
    FOR EACH i IN tour
        j = i + 1
        IF j = 8 THEN j = 0
        s = s + Gd(tour(i) * 8 + tour(j))
    NEXT i
    RETURN s
END FUNCTION

SUB Main()
    DIM cx(7) AS LONG
    DIM cy(7) AS LONG
    cx(0)=0 : cy(0)=0
    cx(1)=2 : cy(1)=1
    cx(2)=5 : cy(2)=0
    cx(3)=6 : cy(3)=3
    cx(4)=3 : cy(4)=4
    cx(5)=1 : cy(5)=5
    cx(6)=7 : cy(6)=6
    cx(7)=4 : cy(7)=7
    FOR i = 0 TO 7
        FOR j = 0 TO 7
            dx = cx(i) - cx(j)
            dy = cy(i) - cy(j)
            Gd(i*8+j) = dx*dx + dy*dy
        NEXT j
    NEXT i

    ' ── 1. Costruttive: cheapest insertion e nearest neighbour ───────────────────────
    DIM t1(7) AS LONG
    DIM t2(7) AS LONG
    ok1 = OPTIMIZE.LSCHEAPEST(Gd, 8, t1)
    c1 = GiroCosto(t1)
    ok2 = OPTIMIZE.LSNEAREST(Gd, 8, 0, t2)
    c2 = GiroCosto(t2)
    PRINT "cheapest="; ok1; ":"; FORMAT(c1, "#");
    PRINT "|nearest="; ok2; ":"; FORMAT(c2, "#")

    ' ── 2. Miglioramento: LSSOLVE (hill climbing) e LSSOLVECFG (3 partenze, 2 acceptor) ──
    DIM o(2) AS DOUBLE
    r = OPTIMIZE.LSSOLVE(GiroCosto, t1, 8, 0, 500, o)
    PRINT "hc="; r; ":"; FORMAT(o(1), "#"); ":"; (o(1) <= c1); ":"; (o(0) = 0)

    DIM best(7) AS LONG
    DIM ob(2) AS DOUBLE
    FOR i = 0 TO 7 : best(i) = t2(i) : NEXT i
    r = OPTIMIZE.LSSOLVECFG(GiroCosto, best, 8, 0, 500, 3, 1, ob)
    PRINT "|la="; r; ":"; FORMAT(ob(1), "#"); ":"; (ob(1) <= c2)

    r = OPTIMIZE.LSSOLVECFG(GiroCosto, best, 8, 0, 500, 3, 3, ob)
    PRINT "|sa="; r; ":"; FORMAT(ob(1), "#"); ":"; (ob(1) <= c2)

    ' ── 3. Sanità: la soluzione migliore è ancora una permutazione di 0..7 ───────────
    DIM visto(7) AS LONG
    perm = 1
    FOR i = 0 TO 7
        v = best(i)
        IF v < 0 OR v > 7 THEN perm = 0
        IF visto(v) <> 0 THEN perm = 0
        visto(v) = 1
    NEXT i
    PRINT "|perm="; perm; ":"; FORMAT(GiroCosto(best), "#")

    ' ── 4. LSNOWMS: orologio CPU monotono (usato per time-box della ricerca) ──────────
    t0 = OPTIMIZE.LSNOWMS()
    FOR i = 1 TO 200000 : NEXT i
    t1b = OPTIMIZE.LSNOWMS()
    PRINT "|clock="; (t0 >= 0); (t1b >= t0)

    ' ── 5. Riepilogo del giro migliore (indici delle città in ordine) ────────────────
    PRINT "|tour="
    FOR i = 0 TO 7
        PRINT best(i);
    NEXT i
    PRINT ""
END SUB
