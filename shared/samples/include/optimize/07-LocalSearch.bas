REM FILE: 07-LocalSearch.bas — pacchetto `localsearch` (FFI): usa TUTTE le 5 funzioni di
REM OPTIMIZE.LS* (LSSOLVE, LSSOLVECFG, LSNEAREST, LSCHEAPEST, LSNOWMS).
REM Scenario: giro del commesso viaggiatore su 8 città su griglia intera. Le distanze sono
REM QUADRATI esatti (aritmetica intera) → obiettivo esatto, nessuna dipendenza dal FP: il
REM risultato è identico sui 4 backend. Output deterministico.
REM
REM ATTENZIONE ALLA CONVENZIONE: il motore MASSIMIZZA il punteggio (gor_ls_score_cmp), quindi
REM un problema di minimo (come il TSP) si scrive restituendo il costo NEGATO.
IMPORT OPTIMIZE

' Distanza al quadrato fra le città (matrice 8x8 row-major, globale: la legge la callback).
DIM Gd(63) AS DOUBLE
DIM NCity AS LONG

' Obiettivo della ricerca locale: NEGATIVO della somma dei quadrati delle distanze lungo il
' giro (il motore massimizza, noi vogliamo il giro più corto).
FUNCTION GiroCosto(tour() AS DOUBLE) AS DOUBLE
    s = 0
    FOR k = 0 TO NCity - 1
        a = tour(k)
        IF k = NCity - 1 THEN
            b = tour(0)
        ELSE
            b = tour(k + 1)
        END IF
        s = s + Gd(a * NCity + b)
    NEXT k
    RETURN -s
END FUNCTION

' Stessa lunghezza sui giri LONG del chiamante (per giudicare le soluzioni restituite da
' LSNEAREST/LSCHEAPEST e dalla ricerca). Tipi allineati: un array LONG passato a un parametro
' DOUBLE è convertito da C99/PCode/VM-C ma non da IL, quindi qui si resta sui LONG.
FUNCTION CostoL(t() AS LONG) AS DOUBLE
    s = 0
    FOR k = 0 TO NCity - 1
        a = t(k)
        IF k = NCity - 1 THEN
            b = t(0)
        ELSE
            b = t(k + 1)
        END IF
        s = s + Gd(a * NCity + b)
    NEXT k
    RETURN s
END FUNCTION

' TRUE se t() è una permutazione esatta di 0..NCity-1.
FUNCTION IsPerm(t() AS LONG) AS LONG
    DIM visto(15) AS LONG
    FOR i = 0 TO NCity - 1
        v = t(i)
        IF v < 0 OR v > NCity - 1 THEN RETURN 0
        IF visto(v) <> 0 THEN RETURN 0
        visto(v) = 1
    NEXT i
    RETURN 1
END FUNCTION

SUB Main()
    NCity = 8
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
    FOR i = 0 TO NCity - 1
        FOR j = 0 TO NCity - 1
            dx = cx(i) - cx(j)
            dy = cy(i) - cy(j)
            Gd(i*NCity+j) = dx*dx + dy*dy
        NEXT j
    NEXT i

    ' ── 1. Costruttive: cheapest insertion e nearest neighbour ───────────────────────
    DIM t1(7) AS LONG
    DIM t2(7) AS LONG
    ok1 = OPTIMIZE.LSCHEAPEST(Gd, NCity, t1)
    c1 = CostoL(t1)
    ok2 = OPTIMIZE.LSNEAREST(Gd, NCity, 0, t2)
    c2 = CostoL(t2)
    PRINT "cheapest="; ok1; ":"; FORMAT(c1, "#"); ":"; IsPerm(t1);
    PRINT "|nearest="; ok2; ":"; FORMAT(c2, "#"); ":"; IsPerm(t2)

    ' ── 2. LSSOLVE: hill climbing da t1 (1 partenza) ─────────────────────────────────
    DIM tour(7) AS LONG
    FOR i = 0 TO NCity - 1 : tour(i) = t1(i) : NEXT i
    DIM o(2) AS DOUBLE
    r = OPTIMIZE.LSSOLVE(GiroCosto, tour, NCity, 0, 500, o)
    PRINT "|hc="; r; ":"; FORMAT(-o(1), "#"); ":"; (o(0) = 0); ":"; IsPerm(tour)

    ' ── 3. LSSOLVECFG: 3 partenze con late acceptance e simulated annealing ──────────
    FOR i = 0 TO NCity - 1 : tour(i) = t2(i) : NEXT i
    DIM ob(2) AS DOUBLE
    r = OPTIMIZE.LSSOLVECFG(GiroCosto, tour, NCity, 0, 500, 3, 1, ob)
    PRINT "|la="; r; ":"; FORMAT(-ob(1), "#"); ":"; IsPerm(tour)

    FOR i = 0 TO NCity - 1 : tour(i) = t2(i) : NEXT i
    r = OPTIMIZE.LSSOLVECFG(GiroCosto, tour, NCity, 0, 500, 3, 3, ob)
    PRINT "|sa="; r; ":"; FORMAT(-ob(1), "#"); ":"; IsPerm(tour)

    ' ── 4. La ricerca non peggiora la costruttiva di partenza (minimizzazione) ────────
    PRINT "|migliora="; (-ob(1) <= c2); ":"; (o(1) <= 0)

    ' ── 5. LSNOWMS: orologio CPU monotono (time-box della ricerca) ───────────────────
    t0 = OPTIMIZE.LSNOWMS()
    FOR i = 1 TO 200000 : NEXT i
    t1b = OPTIMIZE.LSNOWMS()
    PRINT "|clock="; (t0 >= 0); (t1b >= t0)

    ' ── 6. Giro finale (indici delle città nell'ordine di visita) ────────────────────
    PRINT "|tour="
    FOR i = 0 TO NCity - 1
        PRINT tour(i);
    NEXT i
    PRINT "|cost="; FORMAT(CostoL(tour), "#")
    PRINT ""
END SUB
