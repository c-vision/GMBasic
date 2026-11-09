' SURFACE: esercita la superficie del namespace (drill), non uno scenario didattico.
' FILE: Trading/TradingFullCoverage.bas
' Copertura delle funzioni Trading/Portfolio.
' I risultati di PORTFOLIO sono ARRAY: [rendimento, volatilita', Sharpe, pesi...].
IMPORT PORTFOLIO AS TP
IMPORT METRICS AS TM
IMPORT TRADING

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "Trading - Copertura Completa"
    PRINT "============================================================"
    PRINT

    DIM rets(9) AS DOUBLE
    rets(0)=0.05 : rets(1)=0.03 : rets(2)=0.04 : rets(3)=0.02 : rets(4)=0.01
    rets(5)=0.03 : rets(6)=0.02 : rets(7)=0.04 : rets(8)=0.03 : rets(9)=0.02

    ' Le funzioni di rischio storico accettano una MATRICE (periodi x asset).
    DIM hist(0, UBOUND(rets)) AS DOUBLE
    DIM hi AS LONG
    FOR hi = 0 TO UBOUND(rets) : hist(0, hi) = rets(hi) : NEXT hi

    ' --- MAXRETURN(meanReturns) ---
    PRINT "-- MAXRETURN --"
    DIM mr() AS DOUBLE
    mr = TP.MAXRETURN(rets)
    PRINT "MAXRETURN Weights len: "; UBOUND(mr) - 2
    PRINT "MAXRETURN ExpectedReturn: "; FORMAT(mr(0), "0.0000")
    PRINT

    ' --- KELLYCRITERION(returns, riskFree) ---
    PRINT "-- KELLYCRITERION --"
    DIM kc() AS DOUBLE
    kc = TP.KELLYCRITERION(hist, 0.02)
    PRINT "KELLYCRITERION value: "; FORMAT(kc(0), "0.0000")
    PRINT

    ' --- MINCVAR(returns, conf, rf) ---
    PRINT "-- MINCVAR --"
    DIM cv() AS DOUBLE
    cv = TP.MINCVAR(hist, 0.95, 0.02)
    PRINT "MINCVAR value: "; FORMAT(cv(0), "0.0000")
    PRINT

    ' --- MINCDAR(returns, conf, rf) ---
    PRINT "-- MINCDAR --"
    DIM cd() AS DOUBLE
    cd = TP.MINCDAR(hist, 0.95, 0.02)
    PRINT "MINCDAR value: "; FORMAT(cd(0), "0.0000")
    PRINT

    ' --- Metriche singole ---
    PRINT "-- Metriche --"
    PRINT "RISKPREMIUM: "; TRADING.RiskPremium(0.08, 0.02)
    PRINT "CAPITALGAINSYIELD: "; TRADING.CapitalGainsYield(100, 110)
    PRINT "HOLDINGPERIODRETURN: "; TRADING.HoldingPeriodReturn(rets)
    PRINT "GEOMETRICMEANRETURN: "; TRADING.GeometricMeanReturn(rets)
    PRINT "GROWTHRATE: "; TRADING.GrowthRate(0.5, 0.15)
    PRINT "BIDASKSPREAD: "; TRADING.BidAskSpread(99.5, 100.5)
    PRINT "TAXEQUIVALENTYIELD: "; TRADING.TaxEquivalentYield(0.05, 0.30)
    PRINT

    PRINT "Test Completato."
END SUB
