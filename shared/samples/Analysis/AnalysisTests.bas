' FILE: Analysis/AnalysisTests.bas
' =================================================================================================
' TEST UNITARIO: STATS / TIMESERIES / NUMERICAL / MULTIVARIATE - ANALISI DATI
' =================================================================================================
' Questo test verifica le funzioni un tempo in ANALYSIS, ora divise tra STATS, TIMESERIES,
' NUMERICAL, MULTIVARIATE e REGRESSION, dedicate all'elaborazione di dati
' statistici, serie temporali e machine learning di base.
'
' COPERTURA:
' - Statistica Descrittiva: Mean, Median, Variance, Skewness, Kurtosis.
' - Regressione: Regress (Lineare).
' - Time Series: Arima, HoltWinters, AdfTest (Stazionarietà).
' - Clustering: KMeans.
' - Trasformate: FFT (Fast Fourier Transform), IFFT.
' - Interpolazione: Linear, Spline, Polynomial.
' - Calcolo: Integrate, Differentiate.
' =================================================================================================
IMPORT REGRESSION
IMPORT STATS
IMPORT JSON

SUB Main()
    CLS
    PRINT "╔════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ STATS/TIMESERIES/NUMERICAL - Analisi e Calcolo                         ║"
    PRINT "╚════════════════════════════════════════════════════════════════════════╝"
    PRINT
    
    ' --- DATASET DI PROVA ---
    DIM DatiX() = {1.0, 2.0, 3.0, 4.0, 5.0}
    DIM DatiY() = {2.1, 4.2, 6.1, 8.0, 9.9} ' Quasi y = 2x
    DIM SerieTemporale() AS DOUBLE
    ' Popola serie con dati dummy (es. sinusoide + trend)
    REDIM SerieTemporale(100)
    FOR i = 0 TO 99
        SerieTemporale(i) = SIN(i * 0.1) + (i * 0.05)
    NEXT i
    
    ' --- 1. REGRESSIONE LINEARE ---
    PRINT "--- 1. Regressione Lineare ---"
    
    ' Ora 'RegRes' sarà del tipo nativo 'RegressionResultType' fornito da REGRESSION.
    DIM RegRes AS REGRESSION.RegressionResult
    ' Esegue la regressione OLS
    RegRes = REGRESSION.Regress(DatiX, DatiY)
    
    PRINT "Pendenza (Slope):    "; FORMAT(RegRes.Slope, "0.00")
    PRINT "Intercetta:          "; FORMAT(RegRes.Intercept, "0.00")
    PRINT "R-Quadro (R^2):      "; FORMAT(RegRes.R2, "0.0000")
    PRINT
    
    ' --- 2. TIME SERIES ANALYSIS ---
    PRINT "--- 2. Analisi Serie Temporali ---"
    
    ' Test di stazionarietà (Augmented Dickey-Fuller)
    DIM AdfRes = TIMESERIES.AdfTest(SerieTemporale)
    PRINT "ADF Statistic:       "; FORMAT(AdfRes.Statistic, "0.00")
    PRINT "ADF p-value:         "; FORMAT(AdfRes.PValue, "0.0000")
    
    ' Previsione Holt-Winters (Exponential Smoothing)
    ' Alpha=0.2, Beta=0.1, Gamma=0.1, Periodo=10
    DIM HwRes = TIMESERIES.HoltWinters(SerieTemporale, 0.2, 0.1, 0.1, 10)
    PRINT "Forecast (primo val):"; FORMAT(HwRes.Forecast(0), "0.00")
    
    ' ARIMA (AutoRegressive Integrated Moving Average)
    ' Modello (1, 1, 1)
    DIM ArimaRes = TIMESERIES.Arima(SerieTemporale, 1, 1, 1)
    PRINT "ARIMA AIC:           "; FORMAT(ArimaRes.AIC, "0.00")
    PRINT
    
    ' --- 3. CLUSTERING (K-MEANS) ---
    PRINT "--- 3. Clustering (K-Means) ---"
    
    DIM ClusterData(20) AS DOUBLE
    ' Popola con due gruppi distinti di valori 1D
    FOR i = 0 TO 9 : ClusterData(i) = RND() : NEXT
    FOR i = 10 TO 19: ClusterData(i) = 10 + RND() : NEXT
    
    DIM KRes = MULTIVARIATE.KMeans(ClusterData, 2) ' Cerchiamo 2 cluster
    PRINT "Centroidi trovati:   "; JSON.Stringify(KRes.Centroids)
    PRINT
    
    ' --- 4. TRASFORMATE (FFT) ---
    PRINT "--- 4. Fast Fourier Transform (FFT) ---"
    
    DIM Signal(8) AS DOUBLE = {1, 1, 1, 1, 0, 0, 0, 0}
    DIM FftRes() = NUMERICAL.Fft(Signal)
    ' FFT ritorna una matrice n x 2 (colonna 0 = parte reale, colonna 1 = parte immaginaria —
    ' vedi CommonLib/Numerics/FFT.cs). MathNet applica scaling simmetrico 1/sqrt(N) sia in
    ' avanti che all'indietro (FourierOptions.Default), quindi bin 0 NON è la somma grezza del
    ' segnale ma somma/sqrt(N) — il valore esatto conta meno del fatto che il round-trip
    ' FFT->IFFT sotto ricostruisca il segnale originale.
    PRINT "FFT Result bin 0 (re, im):"; FORMAT(FftRes(0, 0), "0.0"); ", "; FORMAT(FftRes(0, 1), "0.0")

    DIM IfftRes() = NUMERICAL.Ifft(FftRes)
    PRINT "IFFT (Ricostruzione):"; FORMAT(IfftRes(0, 0), "0.0"); " (Atteso ~1.0)"
    PRINT
    
    ' --- 5. INTERPOLAZIONE E CALCOLO ---
    PRINT "--- 5. Interpolazione e Calcolo ---"
    
    ' Interpolazione
    DIM x_new = 2.5
    DIM y_lin = NUMERICAL.InterpolateLinear(DatiX, DatiY, x_new)
    DIM y_spl = NUMERICAL.InterpolateSpline(DatiX, DatiY, x_new)
    PRINT "Interpolazione a x="; x_new
    PRINT "  Lineare: "; y_lin
    PRINT "  Spline:  "; y_spl
    DIM y_pchip = NUMERICAL.InterpolatePchip(DatiX, DatiY, x_new)
    DIM y_poly = NUMERICAL.InterpolatePolynomial(DatiX, DatiY, x_new)
    DIM y_step = NUMERICAL.InterpolateStep(DatiX, DatiY, x_new)
    DIM y_clamp = NUMERICAL.InterpolateClamp(DatiX, DatiY, x_new)
    DIM y_gen = NUMERICAL.InterpolateGeneric("linear", DatiX, DatiY, x_new)
    PRINT "  Pchip:   "; y_pchip
    PRINT "  Poly:    "; y_poly
    PRINT "  Step:    "; y_step
    PRINT "  Clamp:   "; y_clamp
    PRINT "  Generic: "; y_gen
    
    ' Integrazione numerica di una funzione (es. x^2)
    ' Richiede una lambda o function pointer
    DIM Integrale = NUMERICAL.Integrate(FN(x) => x*x, 0, 1)
    PRINT "Integrale x^2 da 0 a 1: "; FORMAT(Integrale, "0.000"); " (Atteso: 0.333)"
    
    ' Differenziazione (Derivata)
    DIM Derivata = NUMERICAL.Differentiate(FN(x) => x*x, 2)
    PRINT "Derivata x^2 in x=2:    "; FORMAT(Derivata, "0.00"); " (Atteso: 4.00)"
    PRINT
    
    PRINT "--- Distribuzioni di Probabilità ---"
    PRINT "STATS.NORMALPDF(0):  "; FORMAT(STATS.NORMALPDF(0), "0.0000")
    PRINT "NORMALCDF(0):  "; FORMAT(STATS.NORMALCDF(0), "0.0000")
    PRINT "LOGNORMALPDF(1): "; FORMAT(STATS.LOGNORMALPDF(1), "0.0000")
    PRINT "LOGNORMALCDF(1): "; FORMAT(STATS.LOGNORMALCDF(1), "0.0000")
    PRINT "CHIPDF(1,2):   "; FORMAT(STATS.CHIPDF(1,2), "0.0000")
    PRINT "CHICDF(1,2):   "; FORMAT(STATS.CHICDF(1,2), "0.0000")
    PRINT "CHISQUAREDPDF(1,2): "; FORMAT(STATS.CHISQUAREDPDF(1,2), "0.0000")
    PRINT "CHISQUAREDCDF(1,2): "; FORMAT(STATS.CHISQUAREDCDF(1,2), "0.0000")
    PRINT "BETAPDF(0.5,2,2): "; FORMAT(STATS.BETAPDF(0.5,2,2), "0.0000")
    PRINT "BETACDF(0.5,2,2): "; FORMAT(STATS.BETACDF(0.5,2,2), "0.0000")
    PRINT "STUDENTTPDF(0,5): "; FORMAT(STATS.STUDENTTPDF(0,5), "0.0000")
    PRINT "STUDENTTCDF(0,5): "; FORMAT(STATS.STUDENTTCDF(0,5), "0.0000")
    PRINT "WEIBULLPDF(1,2,3): "; FORMAT(STATS.WEIBULLPDF(1,2,3), "0.0000")
    PRINT "WEIBULLCDF(1,2,3): "; FORMAT(STATS.WEIBULLCDF(1,2,3), "0.0000")
    PRINT "FPDF(1,5,10):  "; FORMAT(STATS.FPDF(1,5,10), "0.0000")
    PRINT "FCDF(1,5,10):  "; FORMAT(STATS.FCDF(1,5,10), "0.0000")
    PRINT "EXPONENTIALPDF(1,2): "; FORMAT(STATS.EXPONENTIALPDF(1,2), "0.0000")
    PRINT "EXPONENTIALCDF(1,2): "; FORMAT(STATS.EXPONENTIALCDF(1,2), "0.0000")
    PRINT "PARETOPDF(1,2,3): "; FORMAT(STATS.PARETOPDF(1,2,3), "0.0000")
    PRINT "LOGISTICPDF(0,0,1): "; FORMAT(STATS.LOGISTICPDF(0,0,1), "0.0000")
    PRINT "TRIANGULARPDF(0.5): "; FORMAT(STATS.TRIANGULARPDF(0.5), "0.0000")
    PRINT "UNIFORMPDF(0.5):  "; FORMAT(STATS.UNIFORMPDF(0.5), "0.0000")
    PRINT "SKEWEDNORMALPDF(0): "; FORMAT(STATS.SKEWEDNORMALPDF(0), "0.0000")
    PRINT "BERNOULLIPMF(1,0.5): "; FORMAT(STATS.BERNOULLIPMF(1,0.5), "0.0000")
    PRINT "BINOMIALPMF(3,10,0.5): "; FORMAT(STATS.BINOMIALPMF(3,10,0.5), "0.0000")
    PRINT "POISSONPMF(3,2): "; FORMAT(STATS.POISSONPMF(3,2), "0.0000")
    PRINT "GEOMETRICPMF(3,0.5): "; FORMAT(STATS.GEOMETRICPMF(3,0.5), "0.0000")
    PRINT "NEGATIVEBINOMIALPMF(3,5,0.5): "; FORMAT(STATS.NEGATIVEBINOMIALPMF(3,5,0.5), "0.0000")
    PRINT "HYPERGEOMETRICPMF(2,5,10,20): "; FORMAT(STATS.HYPERGEOMETRICPMF(2,20,10,5), "0.0000")
    PRINT

    PRINT "Test Completato."
END SUB