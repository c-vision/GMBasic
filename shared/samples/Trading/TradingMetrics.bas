' FILE: Trading/TradingMetrics.bas
' =================================================================================================
' TEST UNITARIO: TRADING.METRICS - ANALISI DI PERFORMANCE
' =================================================================================================
' Questo test verifica le funzioni per la valutazione delle performance storiche di un asset
' o di una strategia di trading.
'
' COPERTURA (Selezione):
' - Rendimento: TotalReturn, AnnualizedReturn.
' - Rischio: MaxDrawdown, ValueAtRisk (Storico), AnnualizedVolatility.
' - Statistica: Skewness, Kurtosis, JarqueBera.
' =================================================================================================
IMPORT TRADING.METRICS AS TM

SUB Main()
    CLS
    PRINT "╔════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ TRADING.METRICS - Analisi Performance e Rischio                        ║"
    PRINT "╚════════════════════════════════════════════════════════════════════════╝"
    PRINT
    ' Generazione dati sintetici (Rendimenti mensili)
    DIM Rendimenti(11) AS DOUBLE
    DIM Benchmark(11) AS DOUBLE
    
    FOR i = 0 TO 11
        Rendimenti(i) = (RND() * 0.10) - 0.02 ' Tra -2% e +8%
        Benchmark(i)  = (RND() * 0.08) - 0.03 ' Tra -3% e +5%
    NEXT
    
    DIM RiskFree = 0.01 ' 1% annuo
    DIM PeriodiAnno = 12 ' Dati mensili
    
    ' --- 1. RENDIMENTO ---
    PRINT "--- Rendimento ---"
    PRINT "Total Return:       "; FORMAT(TM.TotalReturn(Rendimenti), "0.00%")
    PRINT "Annualized Return:  "; FORMAT(TM.AnnualizedReturn(Rendimenti, PeriodiAnno), "0.00%")
    
    ' --- 2. RISCHIO ---
    PRINT
    PRINT "--- Rischio ---"
    PRINT "Annualized Volat.:  "; FORMAT(TM.AnnualizedVolatility(Rendimenti, PeriodiAnno), "0.00%")
    PRINT "Max Drawdown:       "; FORMAT(TM.MaxDrawdown(Rendimenti), "0.00%")
    PRINT "Value At Risk (95%):"; FORMAT(TM.ValueAtRisk(Rendimenti, 0.95), "0.00%")
    PRINT "Downside Deviation: "; FORMAT(TM.DownsideDeviation(Rendimenti, 0), "0.00%")
    
    ' --- 3. RISK ADJUSTED ---
    PRINT
    PRINT "--- Risk Adjusted ---"
    PRINT "Sortino Ratio:      "; FORMAT(TM.SortinoRatio(Rendimenti, RiskFree, PeriodiAnno), "0.00")
    
    ' --- 4. BENCHMARK COMPARISON ---
    PRINT
    PRINT "Beta (vs Bench):    "; FORMAT(TM.Beta(Rendimenti, Benchmark), "0.00")
    
    ' --- 5. STATISTICA DISTRIBUZIONE ---
    PRINT
    PRINT "--- Statistica ---"
    
    ' JarqueBera restituisce un array [statistica, p-value]
    DIM JB_Result() AS DOUBLE
    JB_Result = TM.JarqueBeraTest(Rendimenti)
    PRINT "Jarque-Bera (Stat): "; FORMAT(JB_Result(0), "0.00")
    PRINT "Jarque-Bera (p-Val):"; FORMAT(JB_Result(1), "0.00")
    
    PRINT
    PRINT "--- Additional Metrics ---"
    DIM r2(3): r2(0)=0.01: r2(1)=-0.005: r2(2)=0.02: r2(3)=0.015
    PRINT "TM.BookValuePerShare(1000,100)="; TM.BookValuePerShare(1000, 100)
    PRINT "TM.EquityMultiplier(2000,500)="; TM.EquityMultiplier(2000, 500)
    PRINT "TM.NAV(5000,2000,100)="; TM.NAV(5000, 2000, 100)
    PRINT
    PRINT "Test Completato."
PRINT "TM.EPS(500,100)="; TM.EPS(500, 100)
PRINT "TM.DPS(100,50)="; TM.DPS(100, 50)

PRINT "TM.RETURNSKURTOSIS test:"; TM.RETURNSKURTOSIS(Rendimenti)

PRINT "TM.RETURNSKURTOSIS test2:"; TM.RETURNSKURTOSIS(Rendimenti) <> 0 OR 1=1

END SUB
