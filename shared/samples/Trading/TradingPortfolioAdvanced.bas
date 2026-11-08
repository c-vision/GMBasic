' ==============================================================================
' TradingPortfolioAdvanced.bas — BACKTEST.RUN + PORTFOLIO.EVOLVE/BLACKLITTERMAN
' ==============================================================================
' Copre: BACKTEST.RUN (media per periodo, pesi uguali),
'        PORTFOLIO.EVOLVE, PORTFOLIO.BLACKLITTERMAN.
' I risultati sono ARRAY: [rendimento, volatilita', Sharpe, pesi...].
' ==============================================================================
IMPORT BACKTEST
IMPORT PORTFOLIO AS TP

SUB Main()
    PRINT "=== TRADING avanzato ==="

    ' BACKTEST.RUN: array JAGGED [periodo][asset], rendimento per periodo = media degli asset.
    DIM data(1) AS ARRAY OF DOUBLE
    data(0) = {0.01, 0.03}
    data(1) = {0.02, 0.04}
    DIM bt() AS DOUBLE
    bt = BACKTEST.RUN(data, "")
    PRINT "TotalReturn:    "; ROUND(bt(0), 4)
    PRINT "MaxDrawdown:    "; ROUND(bt(2), 4)
    PRINT "Sharpe:         "; ROUND(bt(3), 4)

    ' PORTFOLIO.EVOLVE: minimizza la varianza (proxy deterministico del GA).
    DIM rets(2) AS DOUBLE : rets = {0.10, 0.05, 0.02}
    DIM cov(2) AS ARRAY OF DOUBLE
    cov(0) = {0.04, 0.005, 0.0} : cov(1) = {0.005, 0.01, 0.0} : cov(2) = {0.0, 0.0, 0.0025}
    DIM ev() AS DOUBLE
    ev = TP.EVOLVE(rets, cov)
    PRINT "EVOLVE weights: "; UBOUND(ev) - 2

    ' BLACKLITTERMAN: caps, cov, lambda, tau, P (view), Q, omega, rf.
    DIM caps(2) AS DOUBLE : caps = {100.0, 200.0, 300.0}
    DIM P(2) AS ARRAY OF DOUBLE
    P(0) = {1.0, 0.0, 0.0} : P(1) = {0.0, 1.0, 0.0} : P(2) = {0.0, 0.0, 1.0}
    DIM Q(2) AS DOUBLE : Q = {0.10, 0.05, 0.02}
    DIM omega(2) AS ARRAY OF DOUBLE
    omega(0) = {0.0001, 0.0, 0.0} : omega(1) = {0.0, 0.0001, 0.0} : omega(2) = {0.0, 0.0, 0.0001}
    DIM bl() AS DOUBLE
    bl = TP.BLACKLITTERMAN(caps, cov, 2.5, 0.05, P, Q, omega, 0.01)
    PRINT "BL weights:     "; UBOUND(bl) - 2
    PRINT "BL volatility:  "; ROUND(bl(1), 6)
    PRINT
    PRINT "Completato."
END SUB
