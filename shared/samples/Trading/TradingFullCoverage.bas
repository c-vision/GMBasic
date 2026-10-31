IMPORT TYPEINFO
' FILE: Trading/TradingFullCoverage.bas
' Copertura completa delle funzioni Trading/Portfolio non ancora coperte.
IMPORT TRADING.PORTFOLIO AS TP
IMPORT TRADING.METRICS AS TM
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

    ' --- MAXRETURN(meanReturns) → PortfolioResult ---
    PRINT "-- MAXRETURN --"
    DIM mr AS TP.PortfolioResult
    mr = TP.MAXRETURN(rets)
    PRINT "MAXRETURN Weights len: "; LEN(mr.Weights)
    PRINT

    ' --- KELLYCRITERION(returns, riskFree) ---
    PRINT "-- KELLYCRITERION --"
    DIM kc AS TP.PortfolioResult
    kc = TP.KELLYCRITERION(rets, 0.02)
    PRINT "KELLYCRITERION type: "; TYPEINFO(kc)
    PRINT

    ' --- MINCVAR(returns, conf, rf) ---
    PRINT "-- MINCVAR --"
    DIM cv AS TP.PortfolioResult
    cv = TP.MINCVAR(rets, 0.95, 0.02)
    PRINT "MINCVAR type: "; TYPEINFO(cv)
    PRINT

    ' --- MINCDAR(returns, conf, rf) ---
    PRINT "-- MINCDAR --"
    DIM cd AS TP.PortfolioResult
    cd = TP.MINCDAR(rets, 0.95, 0.02)
    PRINT "MINCDAR type: "; TYPEINFO(cd)
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
