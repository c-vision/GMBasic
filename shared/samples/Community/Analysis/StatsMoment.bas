' FILE: Analysis/StatsMoment.bas
' =================================================================================================
' TEST UNITARIO: STATS — MOMENTO DI ORDINE N
' =================================================================================================
' `STATS.MOMENT(dati, ordine)` ritorna il momento (centrale) di ordine `ordine` del campione:
' ordine 1 ≈ 0 per costruzione, ordine 2 è la VARIANZA, ordine 3 misura l'asimmetria e
' ordine 4 la curtosi. Serve come mattone per gli indicatori di forma della distribuzione.
'
' COPERTURA:
' - STATS.MOMENT  : momento di ordine 1..4 su un campione deterministico (1..10).
' =================================================================================================
IMPORT STATS

SUB Main()
    DIM d(9) AS DOUBLE
    DIM i AS INTEGER
    DIM somma AS DOUBLE

    ' --- campione deterministico 1..10 (media 5.5)
    FOR i = 0 TO 9
        d(i) = i + 1
        somma = somma + d(i)
    NEXT i

    PRINT "STATS.MOMENT — momenti del campione 1..10"
    PRINT "--------------------------------------------------"
    PRINT "media        = "; somma / 10
    PRINT "momento 1    = "; STATS.MOMENT(d, 1)
    PRINT "momento 2    = "; STATS.MOMENT(d, 2); "  (varianza)"
    PRINT "momento 3    = "; STATS.MOMENT(d, 3); "  (asimmetria)"
    PRINT "momento 4    = "; STATS.MOMENT(d, 4); "  (curtosi)"
    PRINT "--------------------------------------------------"
    PRINT "Fine."
END SUB
