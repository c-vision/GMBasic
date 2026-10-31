' FILE: Trading/TradingMetricsAlpha.bas
' =================================================================================================
' TRADING.METRICS.ALPHA — Alpha di Jensen (rendimento in eccesso rispetto al benchmark).
' =================================================================================================
' ALPHA(rendimenti_asset[], rendimenti_benchmark[], tasso_riskfree, periodi_anno) → DOUBLE
'   = rendimento_asset - [riskfree + beta * (rendimento_benchmark - riskfree)]
' =================================================================================================
IMPORT TRADING.METRICS AS TM
IMPORT MATH

SUB Main()
    CLS
    PRINT "TRADING.METRICS.ALPHA"
    PRINT "======================"
    PRINT

    ' Rendimenti mensili sintetici (12 periodi)
    DIM asset(11) AS DOUBLE = {0.020, 0.035, -0.010, 0.042, 0.008, 0.025, 0.015, -0.005, 0.030, 0.012, 0.018, 0.022}
    DIM bench(11) AS DOUBLE = {0.010, 0.015, -0.008, 0.020, 0.004, 0.012, 0.009, -0.003, 0.014, 0.006, 0.010, 0.011}

    DIM riskfree = 0.01
    DIM periodi = 12

    DIM a = TM.Alpha(asset, bench, riskfree, periodi)
    PRINT "Alpha (annualizzato) = "; MATH.ROUND(a, 6)
    PRINT

    ' Alpha di un asset identico al benchmark: atteso ~0
    DIM b = TM.Alpha(bench, bench, riskfree, periodi)
    PRINT "Alpha (asset = benchmark) = "; MATH.ROUND(b, 6)
END SUB
