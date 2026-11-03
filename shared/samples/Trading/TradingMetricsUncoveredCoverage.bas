' SURFACE: esercita la superficie del namespace (drill), non uno scenario didattico.
' FILE: Trading/TradingMetricsUncoveredCoverage.bas
' =================================================================================================
' TEST UNITARIO: TRADING.METRICS - FUNZIONI NON ANCORA COPERTE DAGLI ESEMPI
' =================================================================================================
' Questo test copre le funzioni TRADING.METRICS che non comparivano in alcun esempio .bas:
'   DILUTEDEPS, PSRATIO, PBRATIO, DIVIDENDPAYOUTRATIO, CALMARRATIO, INFORMATIONRATIO,
'   TREYNORRATIO, CONDITIONALVALUEATRISK, RETURNSSKEWNESS, ANNUALIZEDTRACKINGERROR,
'   ANNUALIZEDDOWNSIDEDEVIATION, OMEGARATIO, ULCERINDEX.
' =================================================================================================
IMPORT TRADING.METRICS AS TM
IMPORT MATH

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "TRADING.METRICS - Funzioni scoperte dagli esempi"
    PRINT "============================================================"
    PRINT

    ' Dati sintetici di rendimento (12 mesi)
    DIM R(11) AS DOUBLE
    DIM B(11) AS DOUBLE
    FOR i = 0 TO 11
        R(i) = (RND() * 0.10) - 0.02
        B(i) = (RND() * 0.08) - 0.03
    NEXT
    DIM riskFree AS DOUBLE = 0.01

    ' ---- Multipli fondamentali ----
    ' NOTA BUG: PSRATIO/PBRATIO/DIVIDENDPAYOUTRATIO sono dichiarate nella firma StdLib
    ' con 0 argomenti ([]) ma il runtime le implementa con 2 argomenti (Prezzo,
    ' VenditePerAzione / ValoreContabilePerAzione / ...). Il linker rifiuta 2
    ' argomenti -> funzioni inutilizzabili finche' la firma StdLib non viene corretta.
    ' Per questo si esegue solo DILUTEDEPS (firma 3 arg coerente) e si documenta.
    PRINT "-- Multipli fondamentali --"
    PRINT "DILUTEDEPS(1000000, 500000, 0)   = "; TM.DILUTEDEPS(1000000, 500000, 0)
    ' PSRATIO(50, 4): bug firma (0 arg in StdLib, 2 richiesti dal runtime)
    ' PBRATIO(50, 8): bug firma (idem)
    ' DIVIDENDPAYOUTRATIO(2, 4): bug firma (idem)
    PRINT


    ' ---- Rapporti di rischio/rendimento ----
    PRINT "-- Rapporti rischio/rendimento --"
    PRINT "CALMARRATIO(R, 12)               = "; TM.CALMARRATIO(R, 12)
    PRINT "INFORMATIONRATIO(R, B, 12)       = "; TM.INFORMATIONRATIO(R, B, 12)
    PRINT "TREYNORRATIO(R, B, riskFree, 12) = "; TM.TREYNORRATIO(R, B, riskFree, 12)
    PRINT "CONDITIONALVALUEATRISK(R, 0.05)  = "; TM.CONDITIONALVALUEATRISK(R, 0.05)
    PRINT

    ' ---- Statistica dei rendimenti ----
    PRINT "-- Statistica rendimenti --"
    PRINT "RETURNSSKEWNESS(R)               = "; TM.RETURNSSKEWNESS(R)
    PRINT "ULCERINDEX(R)                    = "; TM.ULCERINDEX(R)
    PRINT

    ' ---- Misure annualizzate e downside ----
    PRINT "-- Tracking / downside --"
    PRINT "ANNUALIZEDTRACKINGERROR(R,B,12)  = "; TM.ANNUALIZEDTRACKINGERROR(R, B, 12)
    PRINT "ANNUALIZEDDOWNSIDEDEVIATION(R,0,12)= "; TM.ANNUALIZEDDOWNSIDEDEVIATION(R, 0, 12)
    PRINT "OMEGARATIO(R, 0.0)               = "; TM.OMEGARATIO(R, 0.0)
    PRINT

    PRINT "Test Completato."
END SUB