' FILE: Trading/TradingMetricsExtended.bas
' =================================================================================================
' TRADING.METRICS — DividendPayoutRatio, PBRatio, PSRatio + ulteriori multipli fondamentali
' =================================================================================================
IMPORT METRICS AS TM
IMPORT FUNDAMENTALS

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "METRICS — Multipli Fondamentali"
    PRINT "============================================================"
    PRINT

    ' --- DividendPayoutRatio(dividendiPerAzione, utilePerAzione) ---
    DIM dpr = FUNDAMENTALS.DividendPayoutRatio(2.5, 5.0)
    PRINT "DividendPayoutRatio(2.5, 5.0) = "; FORMAT(dpr, "0.00")

    DIM dpr2 = FUNDAMENTALS.DividendPayoutRatio(1.0, 4.0)
    PRINT "DividendPayoutRatio(1.0, 4.0) = "; FORMAT(dpr2, "0.00")

    ' --- PBRatio(prezzoAzione, valoreContabilePerAzione) ---
    DIM pb = FUNDAMENTALS.PBRatio(50.0, 20.0)
    PRINT "PBRatio(50.0, 20.0)           = "; FORMAT(pb, "0.00")

    DIM pb2 = FUNDAMENTALS.PBRatio(100.0, 40.0)
    PRINT "PBRatio(100.0, 40.0)          = "; FORMAT(pb2, "0.00")

    ' --- PSRatio(prezzoAzione, venditePerAzione) ---
    DIM ps = FUNDAMENTALS.PSRatio(30.0, 15.0)
    PRINT "PSRatio(30.0, 15.0)           = "; FORMAT(ps, "0.00")

    DIM ps2 = FUNDAMENTALS.PSRatio(80.0, 10.0)
    PRINT "PSRatio(80.0, 10.0)           = "; FORMAT(ps2, "0.00")

    PRINT
    PRINT "Test Multipli Fondamentali Completato."
END SUB