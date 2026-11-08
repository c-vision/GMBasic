' SURFACE: esercita la superficie del namespace (drill), non uno scenario didattico.
' FILE: Trading/TradingCoreUncoveredCoverage.bas
' =================================================================================================
' TEST UNITARIO: TRADING (CORE) - FUNZIONI NON ANCORA COPERTE DAGLI ESEMPI
' =================================================================================================
' Questo test copre la funzione TRADING core che non compariva in alcun esempio .bas:
'   TOTALSTOCKRETURN.
'
' COPERTURA:
' - TRADING_CORE.TOTALSTOCKRETURN(initialPrice, endPrice, dividends): rendimento totale azionario.
' =================================================================================================
IMPORT TRADING_CORE

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "TRADING (core) - Funzioni scoperte dagli esempi"
    PRINT "============================================================"
    PRINT

    ' 1. TOTALSTOCKRETURN
    PRINT "-- TOTALSTOCKRETURN --"
    ' Acquisto a 100, vendita a 120, dividendi totali 5
    DIM ret AS DOUBLE
    ret = TRADING_CORE.TOTALSTOCKRETURN(100.0, 120.0, 5.0)
    PRINT "TOTALSTOCKRETURN(100, 120, 5) = "; ret
    PRINT

    PRINT "Test Completato."
END SUB