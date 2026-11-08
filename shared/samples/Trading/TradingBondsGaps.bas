' FILE: Trading/TradingBondsGaps.bas
' Copre BONDS.DURATION (7 argomenti) e BONDS.PRICESIMPLE (6 argomenti).
' Chiamate qualificate per essere rintracciabili dal controllo di copertura.
'
' FIRME (CommonLib/Numerics/Finance.cs):
'   DURATION(settlement, maturity, coupon, yld, frequency, basis, modified)
'   PRICESIMPLE(settlement, maturity, rate, yld, redemption, frequency)
' Date come SERIALI double: 2026-01-01 = 46023, 2031-01-01 = 47849.

IMPORT BONDS

SUB Main()
    DIM s AS DOUBLE
    DIM m AS DOUBLE
    s = 46023
    m = 47849

    DIM d1 = BONDS.DURATION(s, m, 0.05, 0.04, 2, 0, FALSE)
    PRINT "DURATION macaulay   = "; d1

    DIM d2 = BONDS.DURATION(s, m, 0.05, 0.04, 2, 0, TRUE)
    PRINT "DURATION modificata = "; d2

    DIM p = BONDS.PRICESIMPLE(s, m, 0.05, 0.04, 100, 2)
    PRINT "PRICESIMPLE         = "; p

    PRINT "TradingBondsGaps: completato."
END SUB
