' FILE: Trading/TradingMetricsExtended.bas
' =================================================================================================
' TRADING.METRICS — DividendPayoutRatio, PBRatio, PSRatio + ulteriori multipli fondamentali
' =================================================================================================
IMPORT TRADING.METRICS AS TM

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "TRADING.METRICS — Multipli Fondamentali"
    PRINT "============================================================"
    PRINT

    ' --- DividendPayoutRatio(dividendiPerAzione, utilePerAzione) ---
    DIM dpr = TM.DIVIDENDPAYOUTRATIO(2.5, 5.0)
    PRINT "DividendPayoutRatio(2.5, 5.0) = "; FORMAT(dpr, "0.00")

    DIM dpr2 = TM.DIVIDENDPAYOUTRATIO(1.0, 4.0)
    PRINT "DividendPayoutRatio(1.0, 4.0) = "; FORMAT(dpr2, "0.00")

    ' --- PBRatio(prezzoAzione, valoreContabilePerAzione) ---
    DIM pb = TM.PBRATIO(50.0, 20.0)
    PRINT "PBRatio(50.0, 20.0)           = "; FORMAT(pb, "0.00")

    DIM pb2 = TM.PBRATIO(100.0, 40.0)
    PRINT "PBRatio(100.0, 40.0)          = "; FORMAT(pb2, "0.00")

    ' --- PSRatio(prezzoAzione, venditePerAzione) ---
    DIM ps = TM.PSRATIO(30.0, 15.0)
    PRINT "PSRatio(30.0, 15.0)           = "; FORMAT(ps, "0.00")

    DIM ps2 = TM.PSRATIO(80.0, 10.0)
    PRINT "PSRatio(80.0, 10.0)           = "; FORMAT(ps2, "0.00")

    PRINT
    PRINT "Test Multipli Fondamentali Completato."
END SUB