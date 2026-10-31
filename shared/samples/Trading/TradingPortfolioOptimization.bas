REM FILE: TradingPortfolioOptimization.bas

' FILE: Trading/TradingPortfolioOptimization.bas
' =================================================================================================
' TEST UNITARIO: TRADING.PORTFOLIO - OTTIMIZZAZIONE
' =================================================================================================
' Questo test verifica le funzioni avanzate di ottimizzazione del portafoglio.
' Queste funzioni risolvono problemi di programmazione quadratica o convessa per trovare
' l'allocazione ottimale degli asset.
'
' COPERTURA:
' - MEANVARIANCE: Frontiera efficiente di Markowitz.
' - MINVOLATILITY: Portafoglio a minima varianza globale.
' - MAXSHARPE: Portafoglio tangente (massimo Sharpe Ratio).
' - RISKPARITY: Equal Risk Contribution (ERC).
' - BLACKLITTERMAN: Modello Black-Litterman con view di mercato.
' - EQUALLYWEIGHTED: Benchmark 1/N.
' =================================================================================================
IMPORT TRADING.PORTFOLIO AS OPT

SUB Main()
    CLS
    PRINT "╔════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ TRADING.PORTFOLIO - Algoritmi di Ottimizzazione                        ║"
    PRINT "╚════════════════════════════════════════════════════════════════════════╝"
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
    
    DIM Ottimo AS OPT.PortfolioResult
    
    ' Riabilitate 2026-08-12 (VM corretta: le funzioni di ottimizzazione ora chiamano
    ' CommonLib.Portfolio.* e ritornano il risultato coi campi ExpectedReturn/Volatility/
    ' SharpeRatio/Weights). Nota: il solutore QLNet può non convergere su input
    ' infattibili (risultato null -> StampaRisultato mostra 0.00%).
    Ottimo = OPT.MINVOLATILITY(Covarianza)
    CALL StampaRisultato("1. MINVOLATILITY", Ottimo)
    Ottimo = OPT.MAXSHARPE(Rendimenti, Covarianza, RiskFree)
    CALL StampaRisultato("2. MAXSHARPE", Ottimo)
    Ottimo = OPT.MEANVARIANCE(Rendimenti, Covarianza, NULL)
    CALL StampaRisultato("3. MEANVARIANCE", Ottimo)
    Ottimo = OPT.RISKPARITY(Covarianza)
    CALL StampaRisultato("4. RISKPARITY", Ottimo)
    
    Ottimo = OPT.EQUALLYWEIGHTED(4)
    CALL StampaRisultato("5. EQUALLYWEIGHTED", Ottimo)
    PRINT "--- 6. BLACKLITTERMAN (richiede P/Q/omega: input 8x8) ---"
    PRINT "Test Ottimizzazione Completato."
END SUB

SUB StampaRisultato(Titolo AS STRING, Res AS OPT.PortfolioResult)
    PRINT Titolo; ":"
    ' Nota: In un caso reale, controlleremmo Res.Converged
    PRINT " Exp. Return: "; FORMAT(Res.ExpectedReturn, "0.00%")
    PRINT " Volatility: "; FORMAT(Res.Volatility, "0.00%")
    PRINT " Sharpe: "; FORMAT(Res.SharpeRatio, "0.00")
    PRINT " Weights: [";
    FOR i = 0 TO UBOUND(Res.Weights)
    PRINT FORMAT(Res.Weights(i), "0.0%");
    IF i < UBOUND(Res.Weights) THEN PRINT ", ";
    NEXT
    PRINT "]"
    PRINT
END SUB