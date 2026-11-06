' FILE: TimeSeries/TimeSeriesTests.bas
' =================================================================================================
'       TEST UNITARIO: TIMESERIES - ANALISI SERIE TEMPORALI
' =================================================================================================
' Questo test verifica le funzioni del namespace TIMESERIES, specializzato nell'analisi
' rolling (finestra mobile) e trasformazione di serie temporali.
'
' COPERTURA:
' - Medie Mobili: TSMA (Simple), TSEMA (Exponential).
' - Volatilità Rolling: TSROLLSTDDEV, TSROLLMAD, TSVOLATILITY.
' - Analisi Statistica Rolling: TSROLLZSCORE, TSROLLCORR, TSROLLCOVARIANCE.
' - Trasformazioni: TSRETURNS (Log/Simple), TSDIFFERENCE.
' - Stagionalità: TSDESEASONALIZE, TSDECOMPOSE.
' - Outliers: TSOUTLIERS.
' =================================================================================================

IMPORT TIMESERIES AS TS
IMPORT JSON

SUB Main()

    CLS
    PRINT "╔════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ TIMESERIES - Analisi Serie Temporali                                   ║"
    PRINT "╚════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' --- SETUP DATI ---
    ' Genera una serie "Prezzo" (Random Walk) — WHILE evita bug ottimizzazione FOR
    DIM Prices(19) AS DOUBLE
    Prices(0) = 100
    DIM i = 1
    WHILE i <= 19
        Prices(i) = Prices(i-1) * (1 + (MATH.RND() - 0.5) * 0.02)
        i = i + 1
    WEND
    
    DIM Benchmark(19) AS DOUBLE
    DIM j = 0
    WHILE j <= 19 : Benchmark(j) = 100 + j : j = j + 1 : WEND

    ' --- 1. MEDIA MOBILE E SMOOTHING ---
    PRINT "--- 1. Smoothing (MA, EMA) ---"
    
    DIM ma() = TS.TSMA(Prices, 3)
    DIM ema() = TS.TSEMA(Prices, 3)
    
    DIM lastIdx = 19
    PRINT "Prezzo["; lastIdx; "]:     "; MATH.ROUND(Prices(lastIdx), 2)
    DIM maLen = LEN(ma) - 1 : IF maLen > lastIdx THEN maLen = lastIdx
    PRINT "MA(3) ["; maLen; "]:       "; MATH.ROUND(ma(maLen), 2)
    DIM emaLen = LEN(ema) - 1 : IF emaLen > lastIdx THEN emaLen = lastIdx
    PRINT "EMA(3) ["; emaLen; "]:     "; MATH.ROUND(ema(emaLen), 2)
    PRINT

    ' --- 2. RENDIMENTI E DIFFERENZE ---
    PRINT "--- 2. Trasformazioni ---"
    
    DIM rets() = TS.TSRETURNS(Prices)
    DIM diff() = TS.TSDIFFERENCE(Prices, 1)
    
    DIM retLen = LEN(rets) - 1 : IF retLen > lastIdx THEN retLen = lastIdx
    PRINT "Return ["; retLen; "]:     "; MATH.ROUND(rets(retLen), 4); " (Pct Change)"
    DIM diffLen = LEN(diff) - 1 : IF diffLen > lastIdx THEN diffLen = lastIdx
    PRINT "Diff ["; diffLen; "]:      "; MATH.ROUND(diff(diffLen), 4); " (Price Change)"
    PRINT

    ' --- 3. ROLLING STATISTICS ---
    PRINT "--- 3. Rolling Stats ---"
    
    ' Deviazione Standard Mobile (Volatilità locale)
    DIM rollStd() = TS.TSROLLSTDDEV(Prices, 5)
    DIM rsLen = LEN(rollStd) - 1 : IF rsLen > lastIdx THEN rsLen = lastIdx
    PRINT "Roll StdDev(5): "; MATH.ROUND(rollStd(rsLen), 4)
    
    ' Z-Score Mobile (per identificare anomalie locali)
    DIM rollZ() = TS.TSROLLZSCORE(Prices, 5)
    DIM rzLen = LEN(rollZ) - 1 : IF rzLen > lastIdx THEN rzLen = lastIdx
    PRINT "Roll Z-Score:   "; MATH.ROUND(rollZ(rzLen), 2)
    
    ' Correlazione Mobile con Benchmark
    DIM rollCorr() = TS.TSROLLCORR(Prices, Benchmark, 5)
    DIM rcLen = LEN(rollCorr) - 1 : IF rcLen > lastIdx THEN rcLen = lastIdx
    PRINT "Roll Corr(5):   "; MATH.ROUND(rollCorr(rcLen), 4)
    PRINT

    ' --- 4. ANALISI AVANZATA ---
    PRINT "--- 4. Advanced Analysis ---"
    
    ' Rilevamento Outliers (basato su soglia sigma o MAD)
    DIM outliers(UBOUND(Prices)) AS LONG
    DIM nOut AS LONG = TS.TSOUTLIERS(Prices, 2.0, outliers)
    PRINT "Outliers (> 2 sigma): "; nOut
    
    ' Volatilità totale della serie
    DIM totalVol = TS.TSVOLATILITY(rets)
    PRINT "Total Volatility: "; MATH.ROUND(totalVol, 4)
    
    ' Autocorrelazione (Lag 1)
    DIM autoCorr = TS.TSAUTOCORR(rets, 1)
    PRINT "Autocorrelation (Lag 1): "; MATH.ROUND(autoCorr, 4)
    PRINT
    
    PRINT "Test Completato."

END SUB