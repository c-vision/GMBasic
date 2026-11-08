REM FILE: TradingPortfolioOptimization.bas

' FILE: Trading/TradingPortfolioOptimization.bas
' =================================================================================================
' PORTFOLIO - OTTIMIZZAZIONE
' =================================================================================================
' Le funzioni di ottimizzazione ritornano un ARRAY:
'   [rendimento atteso, volatilita', Sharpe, pesi 0..n-1]
'
' COPERTURA:
' - MINVOLATILITY: minima varianza globale.
' - MAXSHARPE: portafoglio tangente.
' - MEANVARIANCE: frontiera di Markowitz (rf = 0).
' - RISKPARITY: Equal Risk Contribution (ERC).
' - EQUALLYWEIGHTED: benchmark 1/N.
' =================================================================================================
IMPORT PORTFOLIO AS OPT

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "PORTFOLIO - Algoritmi di Ottimizzazione"
    PRINT "============================================================"
    PRINT

    ' Dati di input simulati
    DIM Rendimenti(3) AS DOUBLE = {0.12, 0.08, 0.05, 0.03}
    ' Matrice di covarianza 4x4 (jagged)
    DIM Covarianza(3) AS ARRAY OF DOUBLE
    Covarianza(0) = {0.04, 0.01, 0.005, 0.0}
    Covarianza(1) = {0.01, 0.02, 0.008, 0.002}
    Covarianza(2) = {0.005, 0.008, 0.01, 0.001}
    Covarianza(3) = {0.0, 0.002, 0.001, 0.005}
    DIM RiskFree = 0.02

    DIM Ottimo() AS DOUBLE

    Ottimo = OPT.MINVOLATILITY(Covarianza)
    CALL StampaRisultato("1. MINVOLATILITY", Ottimo)
    Ottimo = OPT.MAXSHARPE(Rendimenti, Covarianza, RiskFree)
    CALL StampaRisultato("2. MAXSHARPE", Ottimo)
    Ottimo = OPT.MEANVARIANCE(Rendimenti, Covarianza)
    CALL StampaRisultato("3. MEANVARIANCE", Ottimo)
    Ottimo = OPT.RISKPARITY(Covarianza)
    CALL StampaRisultato("4. RISKPARITY", Ottimo)

    Ottimo = OPT.EQUALLYWEIGHTED(4)
    CALL StampaRisultato("5. EQUALLYWEIGHTED", Ottimo)
    PRINT "--- 6. BLACKLITTERMAN (richiede P/Q/omega) ---"
    PRINT "Test Ottimizzazione Completato."
END SUB

SUB StampaRisultato(Titolo AS STRING, Res() AS DOUBLE)
    PRINT Titolo; ":"
    PRINT " Exp. Return: "; FORMAT(Res(0), "0.00%")
    PRINT " Volatility: "; FORMAT(Res(1), "0.00%")
    PRINT " Sharpe: "; FORMAT(Res(2), "0.00")
    PRINT " Weights: [";
    DIM ultimo AS LONG
    ultimo = UBOUND(Res)
    FOR i = 3 TO ultimo
        PRINT FORMAT(Res(i), "0.0%");
        IF i < ultimo THEN PRINT ", ";
    NEXT
    PRINT "]"
    PRINT
END SUB
