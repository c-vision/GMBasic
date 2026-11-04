' FILE: TimeSeries/TimeSeriesAdvanced.bas
' =================================================================================================
'       TEST UNITARIO: TIMESERIES - ANALISI AVANZATA DI SERIE TEMPORALI
' =================================================================================================
' Questo test verifica le funzioni più complesse del namespace TIMESERIES, che richiedono
' più serie di dati o producono output strutturati.
'
' COPERTURA:
' - Correlazione e Covarianza Rolling: TSROLLCORR, TSROLLCOVARIANCE.
' - Regressione Rolling: TSROLLREGRESSION (calcola Alpha e Beta in una finestra mobile).
' - Decomposizione: TSDECOMPOSE (scompone una serie in trend, stagionalità e residuo).
' =================================================================================================

IMPORT TIMESERIES AS TS
IMPORT JSON

SUB Main()

    CLS
    PRINT "╔════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ TIMESERIES - Analisi Avanzata                                          ║"
    PRINT "╚════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' --- SETUP DATI ---
    DIM SerieA(29) AS DOUBLE
    DIM SerieB(29) AS DOUBLE
    DIM Periodo = 10 ' Finestra mobile
    
    ' Serie A: Sinusoide
    ' Serie B: Cosenoide (correlazione negativa e variabile con A)
    DIM idx = 0
    WHILE idx <= 29
        SerieA(idx) = SIN(idx * 0.5)
        SerieB(idx) = COS(idx * 0.5)
        idx = idx + 1
    WEND
    
    ' --- 1. CORRELAZIONE E COVARIANZA ROLLING ---
    PRINT "--- 1. Correlazione e Covarianza Rolling ---"
    
    DIM rollCorr() = TS.TSROLLCORR(SerieA, SerieB, Periodo)
    DIM rollCov() = TS.TSROLLCOVARIANCE(SerieA, SerieB, Periodo)
    
    DIM rcLen = LEN(rollCorr) - 1
    PRINT "Correlazione Rolling (ultimo valore):  "; MATH.ROUND(rollCorr(rcLen), 4)
    PRINT "Covarianza Rolling (ultimo valore):   "; MATH.ROUND(rollCov(rcLen), 4)
    PRINT

    ' --- 2. REGRESSIONE ROLLING ---
    PRINT "--- 2. Regressione Rolling (Alpha & Beta) ---"
    DIM rollReg() AS ARRAY OF DOUBLE = TS.TSROLLREGRESSION(SerieA, SerieB, Periodo)
    DIM regLen = LEN(rollReg) - 1
    DIM ultimoBeta = rollReg(regLen)(1)
    PRINT "Beta Rolling (ultimo valore):         "; MATH.ROUND(ultimoBeta, 4)
    PRINT

    ' --- 3. DECOMPOSIZIONE (TREND, STAGIONALITÀ, RESIDUO) ---
    PRINT "--- 3. Decomposizione Serie Temporale ---"
    DIM SerieStagionale(49) AS DOUBLE
    DIM si = 0
    WHILE si <= 49
        SerieStagionale(si) = (si * 0.1) + SIN(si * 2 * MATH.PI() / 12) * 5 + (MATH.RND() - 0.5)
        si = si + 1
    WEND
    DIM Decomposed() AS ARRAY OF DOUBLE = TS.TSDECOMPOSE(SerieStagionale, 12)
    LET Trend() = Decomposed(0)
    DIM Seasonal() = Decomposed(1)
    DIM Residual() = Decomposed(2)
    DIM decLen = LEN(Trend) - 1
    PRINT "Decomposizione (ultimi valori):"
    PRINT "  Trend:      "; MATH.ROUND(Trend(decLen), 4)
    PRINT "  Stagionalità:"; MATH.ROUND(Seasonal(decLen), 4)
    PRINT "  Residuo:    "; MATH.ROUND(Residual(decLen), 4)
    
    PRINT
    PRINT "Test Completato."

END SUB
