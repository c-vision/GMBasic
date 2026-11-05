' FILE: TimeSeries/AnalisiPrezzi.bas
' SCENARIO DIDATTICO — andamento di un prezzo con TIMESERIES.
'
' Un analista ha una serie di prezzi giornalieri e vuole due indicatori rapidi:
' la VARIAZIONE mobile (TSROLLDELTA) e l'AUTOCORRELAZIONE mobile (TSROLLAUTOCORR),
' per capire direzione e persistenza del movimento. Il report è deterministico.
'
' Domini: TIMESERIES + MATH + base.
IMPORT TIMESERIESX

SUB Main()
    ' 24 prezzi giornalieri: trend crescente con oscillazione regolare.
    DIM prezzi(23) AS DOUBLE
    DIM i AS LONG
    FOR i = 0 TO 23
        prezzi(i) = 100.0 + i * 0.5 + (i MOD 3)
    NEXT

    PRINT "prezzo iniziale: "; MATH.ROUND(prezzi(0), 2)
    PRINT "prezzo finale:   "; MATH.ROUND(prezzi(23), 2)

    ' Variazione mobile su una finestra di 5 giorni.
    DIM delta() AS DOUBLE
    delta() = TIMESERIES.TSROLLDELTA(prezzi, 5)
    PRINT "variazioni calcolate: "; UBOUND(delta) + 1
    PRINT "ultima variazione:    "; MATH.ROUND(delta(UBOUND(delta)), 4)

    ' Autocorrelazione mobile (lag 1): quanto il movimento di oggi ricorda ieri.
    DIM ac() AS DOUBLE
    ac() = TIMESERIES.TSROLLAUTOCORR(prezzi, 10, 1)
    PRINT "autocorrelazioni:     "; UBOUND(ac) + 1
    PRINT "ultima autocorrelazione: "; MATH.ROUND(ac(UBOUND(ac)), 4)
END SUB
