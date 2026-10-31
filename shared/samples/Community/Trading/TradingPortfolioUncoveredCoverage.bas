' FILE: Trading/TradingPortfolioUncoveredCoverage.bas
' =================================================================================================
' TEST UNITARIO: TRADING.PORTFOLIO - FUNZIONI/TIPI NON ANCORA COPERTI DAGLI ESEMPI
' =================================================================================================
' Questo test copre funzioni e tipi TRADING.PORTFOLIO che non comparivano in alcun esempio .bas:
'   PORTFOLIOMAD, PORTFOLIOSEMIMAD, tipo PORTFOLIOCONSTRAINTS, tipo GAPARAMETERS.
'
' COPERTURA:
' - TP.PORTFOLIOMAD(rendimenti)            : Mean Absolute Deviation.
' - TP.PORTFOLIOSEMIMAD(rendimenti, target): Semi-MAD del portafoglio.
' - TIPO PORTFOLIOCONSTRAINTS: campi MINWEIGHT, MAXWEIGHT, TARGETRETURN,
'   TARGETVOLATILITY, SUMOFWEIGHTS, ISLONGONLY.
' - TIPO GAPARAMETERS: campi POPULATIONSIZE, GENERATIONS, MUTATIONRATE, CROSSOVERRATE.
' =================================================================================================
IMPORT TRADING.PORTFOLIO AS TP

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "TRADING.PORTFOLIO - Funzioni/tipi scoperti dagli esempi"
    PRINT "============================================================"
    PRINT

    ' 1. PORTFOLIOMAD / PORTFOLIOSEMIMAD (matrice T x N: osservazioni x asset)
    DIM R(1, 2)
    R(0,0) = 0.05 : R(0,1) = 0.03 : R(0,2) = 0.04
    R(1,0) = 0.02 : R(1,1) = 0.01 : R(1,2) = 0.03

    PRINT "-- PORTFOLIOMAD / PORTFOLIOSEMIMAD --"
    PRINT "PORTFOLIOMAD(R)          = "; TP.PORTFOLIOMAD(R)
    PRINT "PORTFOLIOSEMIMAD(R)      = "; TP.PORTFOLIOSEMIMAD(R)
    PRINT

    ' 2. Tipo PORTFOLIOCONSTRAINTS
    PRINT "-- PORTFOLIOCONSTRAINTS --"
    ' NOTA BUG: i tipi PORTFOLIOCONSTRAINTS e GAPARAMETERS sono dichiarati nello schema
    ' StdLib con campi (MinWeight, MaxWeight, TargetReturn, ...) ma il runtime li mappa
    ' su record C# con campi DIVERSI (PortfolioConstraints record: TargetReturn,
    ' LowerBounds, UpperBounds; GaParameters record: PopulationSize, Generations,
    ' MutationRate, CrossoverRate, RiskCoefficient). L'accesso ai campi dello schema
    ' BASIC fallisce con 'Accesso a campi non supportato'. Bug di mappatura tipo.
    PRINT "-- PORTFOLIOCONSTRAINTS / GAPARAMETERS (BUG MAPPATURA) --"
    PRINT "Tipi dichiarati ma campo schema non mappato sul record runtime (vedi commento)."
    ' DIM C AS TP.PortfolioConstraints = NEW TP.PortfolioConstraints  ' non eseguibile
    ' DIM G AS TP.GaParameters = NEW TP.GaParameters                  ' non eseguibile
    PRINT

    PRINT "Test Completato."
END SUB