' FILE: Trading/TradingPortfolioUncoveredCoverage.bas
' =================================================================================================
' PORTFOLIO — funzioni non coperte dagli esempi principali
' =================================================================================================
' Copre PORTFOLIOMAD e PORTFOLIOSEMIMAD sulla serie dei rendimenti del portafoglio.
' (I tipi PORTFOLIOCONSTRAINTS/GAPARAMETERS del vecchio builtin non esistono piu':
' le funzioni FFI non usano vincoli ne' parametri GA — vedi la guida QUANT.)
' =================================================================================================
IMPORT PORTFOLIO AS TP

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "PORTFOLIO - Funzioni scoperte dagli esempi"
    PRINT "============================================================"
    PRINT

    ' Serie di rendimenti del portafoglio (un valore per periodo).
    DIM R(5) AS DOUBLE
    R(0) = 0.05 : R(1) = 0.03 : R(2) = -0.02 : R(3) = 0.04 : R(4) = 0.01 : R(5) = -0.01

    PRINT "-- PORTFOLIOMAD / PORTFOLIOSEMIMAD --"
    PRINT "PORTFOLIOMAD(R)          = "; FORMAT(TP.PORTFOLIOMAD(R), "#.000000")
    PRINT "PORTFOLIOSEMIMAD(R)      = "; FORMAT(TP.PORTFOLIOSEMIMAD(R), "#.000000")
    PRINT "PORTFOLIOSEMIMAD(R, 0.02)= "; FORMAT(TP.PORTFOLIOSEMIMAD(R, 0.02), "#.000000")
    PRINT

    PRINT "Test Completato."
END SUB
