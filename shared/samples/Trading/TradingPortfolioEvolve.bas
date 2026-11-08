' FILE: TradingPortfolioEvolve.bas
' =================================================================================================
' PORTFOLIO.EVOLVE — minimo di varianza (soluzione deterministica)
' =================================================================================================
' Il vecchio EVOLVE usava un algoritmo genetico con GaParameters: l'FFI non ha il tipo
' e usa la soluzione deterministica di minima varianza, la stessa del runtime condiviso.
' =================================================================================================
IMPORT PORTFOLIO AS TP

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "PORTFOLIO — EVOLVE (minima varianza deterministica)"
    PRINT "============================================================"
    PRINT

    ' 3 asset: rendimenti attesi
    DIM r(2) AS DOUBLE : r = {0.10, 0.05, 0.02}

    ' Matrice di covarianza 3x3
    DIM cov(2) AS ARRAY OF DOUBLE
    cov(0) = {0.04, 0.005, 0.0}
    cov(1) = {0.005, 0.01, 0.0}
    cov(2) = {0.0, 0.0, 0.0025}

    ' Risultato: [rendimento, volatilita', Sharpe, pesi...]
    DIM res1() AS DOUBLE
    res1 = TP.EVOLVE(r, cov)
    PRINT "--- Evolve ---"
    PRINT "Weights: "; UBOUND(res1) - 2; " asset"
    PRINT "Return : "; FORMAT(res1(0), "0.00%")
    PRINT "StDev  : "; FORMAT(res1(1), "0.00%")
    PRINT "Sharpe : "; FORMAT(res1(2), "0.00")
    PRINT
    PRINT "Pesi:  "; FORMAT(res1(3), "0.0000"); " / "; FORMAT(res1(4), "0.0000"); " / "; FORMAT(res1(5), "0.0000")

    PRINT
    PRINT "Test EVOLVE completato."
END SUB
