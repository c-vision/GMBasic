REM FILE: AnalysisStatsDescriptive.bas

' FILE: ANALYSIS/AnalysisStatsDescriptive.bas
' =================================================================================================
'       TEST UNITARIO: ANALYSIS & STATS - STATISTICA DESCRITTIVA
' =================================================================================================
' Questo test verifica le funzioni di base per il calcolo delle statistiche descrittive
' su un array di dati, fornite dai namespace ANALYSIS e STATS.
'
' COPERTURA:
' - Misure di Tendenza Centrale: Mean, Median.
' - Misure di Dispersione: Variance, StdDev, Mad (Median Absolute Deviation).
' - Misure di Forma: Skewness, Kurtosis.
' - Correlazione: Correlation.
' - Quantili: Percentile.
' =================================================================================================

' NOTA: ANALYSIS contiene le funzioni statistiche avanzate, STATS quelle descrittive di base.
IMPORT STATS
IMPORT JSON

SUB Main()

    CLS
    PRINT "╔════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ ANALYSIS & STATS - Statistica Descrittiva                              ║"
    PRINT "╚════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' --- SETUP DATI ---
    DIM Dataset1() = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0}
    DIM Dataset2() = {10.0, 9.0, 8.0, 7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0} ' Correlazione inversa
    DIM DatasetSkew() = {1, 2, 2, 3, 3, 3, 4, 4, 10} ' Coda a destra
    
    PRINT "Dataset 1: "; JSON.Stringify(Dataset1)
    PRINT

    ' --- 1. MISURE DI TENDENZA CENTRALE ---
    PRINT "--- 1. Misure di Tendenza Centrale ---"
    
    PRINT "Media (Mean):         "; STATS.Mean(Dataset1)
    PRINT "Mediana (Median):     "; STATS.Median(Dataset1)
    PRINT

    ' --- 2. MISURE DI DISPERSIONE ---
    PRINT "--- 2. Misure di Dispersione ---"
    
    PRINT "Varianza (Variance):  "; MATH.ROUND(STATS.Variance(Dataset1), 4)
    PRINT "Dev. Standard (StdDev): "; MATH.ROUND(STATS.StdDev(Dataset1), 4)
    PRINT "MAD (Median Abs Dev): "; STATS.MAD(Dataset1)
    PRINT

    ' --- 3. MISURE DI FORMA E CORRELAZIONE ---
    PRINT "--- 3. Misure di Forma e Correlazione ---"
    
    PRINT "Asimmetria (Skewness):"; MATH.ROUND(STATS.Skewness(DatasetSkew), 4)
    PRINT "Curtosi (Kurtosis):   "; MATH.ROUND(STATS.Kurtosis(Dataset1), 4)
    
    DIM Corr = STATS.Correlation(Dataset1, Dataset2)
    PRINT "Correlazione:         "; Corr; " (Atteso: -1)"
    PRINT

    ' --- 4. QUANTILI ---
    PRINT "--- 4. Quantili ---"
    
    ' Percentile 90
    DIM P90 = STATS.PERCENTILE(Dataset1, 0.90)
    PRINT "90° Percentile:       "; P90
    
    ' Mediana come 50° percentile
    DIM P50 = STATS.PERCENTILE(Dataset1, 0.50)
    PRINT "50° Percentile:       "; P50; " (Dovrebbe coincidere con la Mediana)"
    PRINT

    PRINT "Test Completato."
    
END SUB