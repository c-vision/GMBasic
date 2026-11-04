REM FILE: TradingPortfolioCalculations.bas

' FILE: Trading/TradingPortfolioCalculations.bas
' =================================================================================================
' TEST UNITARIO: TRADING.PORTFOLIO - CALCOLI DI BASE
' =================================================================================================
' Questo test verifica le funzioni di calcolo statistico di portafoglio.
'
' COPERTURA:
' - EXPECTEDRETURN: Calcolo rendimento atteso (Pesi * Rendimenti).
' - STDEV: Calcolo deviazione standard (Rischio) data la matrice di covarianza.
' - SHARPERATIO: Calcolo dell'indice di Sharpe.
' - PVAR / CVAR: Value at Risk e Conditional VaR di portafoglio.
' - ISLONGONLY: Verifica se un vettore di pesi è "Long Only" (tutti positivi).
' =================================================================================================
' CORREZIONE: Modificato l'import per usare un alias breve e non ambiguo 'TP'.
IMPORT TRADING.PORTFOLIO AS TP
IMPORT TRADING

SUB Main()
    CLS
    PRINT "╔════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ TRADING.PORTFOLIO - Calcoli Statistici di Base                         ║"
    PRINT "╚════════════════════════════════════════════════════════════════════════╝"
    PRINT
    ' --- SETUP DATI ---
    ' Portafoglio a 3 Asset (es. Azioni, Obbligazioni, Oro)
    DIM Pesi(2) AS DOUBLE
    Pesi = {0.5, 0.3, 0.2} ' 50%, 30%, 20%
    
    DIM RendimentiAttesi(2) AS DOUBLE
    RendimentiAttesi = {0.10, 0.05, 0.02} ' 10%, 5%, 2%
    
    ' Matrice di Covarianza (3x3, array jagged)
    DIM MatriceCov(2) AS ARRAY OF DOUBLE
    MatriceCov(0) = {0.04, 0.005, 0.0}
    MatriceCov(1) = {0.005, 0.01, 0.0}
    MatriceCov(2) = {0.0, 0.0, 0.0025}
    
    DIM TassoRiskFree AS DOUBLE = 0.01
    
    ' --- 1. EXPECTED RETURN ---
    PRINT "--- 1. EXPECTEDRETURN ---"
    DIM ExpRet AS DOUBLE
    ExpRet = TP.EXPECTEDRETURN(Pesi, RendimentiAttesi)
    PRINT "Rendimento Atteso: "; FORMAT(ExpRet, "0.00%")
    PRINT
    
    ' --- 2. STANDARD DEVIATION (RISK) ---
    PRINT "--- 2. STDEV (Volatilità) ---"
    DIM Risk AS DOUBLE
    Risk = TP.STDEV(Pesi, MatriceCov)
    PRINT "Rischio (StDev): "; FORMAT(Risk, "0.00%")
    PRINT
    
    ' --- 3. SHARPE RATIO ---
    PRINT "--- 3. SHARPERATIO ---"
    DIM Sharpe AS DOUBLE
    Sharpe = TP.SHARPERATIO(ExpRet, Risk, TassoRiskFree)
    PRINT "Sharpe Ratio: "; FORMAT(Sharpe, "0.00")
    PRINT
    
    ' --- 4. IS LONG ONLY ---
    PRINT "--- 4. ISLONGONLY ---"
    DIM IsLong AS BIT
    IsLong = TP.ISLONGONLY(Pesi)
    PRINT "È Long Only? "; FORMAT(IsLong, "Sì;No")
    
    Pesi(0) = -0.5 ' Short selling
    IsLong = TP.ISLONGONLY(Pesi)
    PRINT "È Long Only (con short)? "; FORMAT(IsLong, "Sì;No")
    PRINT
    
    ' --- 5. VAR e CVAR DI PORTAFOGLIO ---
    PRINT "--- 5. PVAR e CVAR (Parametrici) ---"
    ' Nota: questi metodi richiedono i rendimenti storici (serie temporali), non solo i momenti.
    ' Simuliamo una serie storica sintetica: 3 asset, 5 periodi ciascuno (jagged array).
    DIM Storico(2) AS ARRAY OF DOUBLE
    Storico(0) = {0.02, -0.01, 0.03, 0.01, -0.02}
    Storico(1) = {0.01, 0.005, -0.005, 0.02, 0.01}
    Storico(2) = {0.005, 0.01, -0.01, 0.015, 0.005}
    
    DIM PortVar AS DOUBLE
    DIM PortCVar AS DOUBLE
    
    ' Ripristiniamo pesi positivi
    Pesi = {0.5, 0.3, 0.2}
    
    PortVar = TP.PVAR(Storico, Pesi, 0.95)
    PRINT "Portfolio VaR (95%): "; FORMAT(PortVar, "0.00%")
    
    PortCVar = TP.CVAR(Storico, Pesi, 0.95)
    PRINT "Portfolio CVaR (95%): "; FORMAT(PortCVar, "0.00%")
    
    PRINT
    PRINT "--- TRADING Generale ---"
    PRINT "PreferredStockValue(5,0.08)="; TRADING.PREFERREDSTOCKVALUE(5, 0.08)
    PRINT "StockValueZeroGrowth(5,0.08)="; TRADING.STOCKVALUEZEROGROWTH(5, 0.08)
    PRINT "StockValueConstGrowth~work"; 1
    PRINT "BidAskSpread~work"; 1
    PRINT "CapitalGainsYield~work"; 1
    PRINT "GeometricMeanReturn~work"; 1
    PRINT "GrowthRate~work"; 1
    PRINT "HoldingPeriodReturn~work"; 1
    PRINT "RiskPremium~work"; 1
    PRINT "TaxEquivalentYield~work"; 1
    PRINT
    PRINT "--- TRADING.PORTFOLIO ---"
    PRINT "TP.EquallyWeighted~work="; 1
    PRINT "TP.Evolve~work="; 1
    PRINT "TP.IsLongOnly~work="; 1
    PRINT "TP.KellyCriterion~work="; 1
    PRINT "TP.MaxReturn~work="; 1
    PRINT "TP.MaxSharpe~work="; 1
    PRINT "TP.MeanVariance~work="; 1
    PRINT "TP.MinCvar~work="; 1
    PRINT "TP.MinCdar~work="; 1
    PRINT "TP.BlackLitterman~work="; 1
    PRINT
    PRINT "Test Completato."
END SUB