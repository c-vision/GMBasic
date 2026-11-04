' FILE: Trading/TradingPortfolioEvolve.bas
' =================================================================================================
' TRADING.PORTFOLIO.EVOLVE — ottimizzazione con Differential Evolution (GA parametri opzionali)
' =================================================================================================
IMPORT TRADING.PORTFOLIO AS TP

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "TRADING.PORTFOLIO — EVOLVE (Differential Evolution)"
    PRINT "============================================================"
    PRINT

    ' 3 asset: rendimenti attesi
    DIM r(2) AS DOUBLE : r = {0.10, 0.05, 0.02}

    ' Matrice di covarianza 3x3
    DIM cov(2) AS ARRAY OF DOUBLE
    cov(0) = {0.04, 0.005, 0.0}
    cov(1) = {0.005, 0.01, 0.0}
    cov(2) = {0.0, 0.0, 0.0025}

    ' EVOLVE con parametri GA predefiniti (NULL)
    DIM res1 = TP.EVOLVE(r, cov, NULL)
    PRINT "--- Evolve (parametri default) ---"
    PRINT "Weights: "; LEN(res1.Weights); " asset"
    PRINT "Return : "; FORMAT(res1.Return, "0.00%")
    PRINT "StDev  : "; FORMAT(res1.StDev, "0.00%")
    PRINT "Sharpe : "; FORMAT(res1.SharpeRatio, "0.00")
    PRINT

    ' EVOLVE con parametri GA espliciti (se il tipo GaParameters è supportato)
    PRINT "--- Evolve (parametri GA via NEW) ---"
    PRINT "NOTA: se GaParameters non è mappato al costruttore BASIC, usare NULL."
    ' DIM ga AS TP.GaParameters = NEW TP.GaParameters(100, 200, 0.8, 0.9, 1.0)
    ' DIM res2 = TP.EVOLVE(r, cov, ga)

    PRINT
    PRINT "Test EVOLVE completato."
END SUB