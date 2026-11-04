' FILE: Trading/TradingBondsGaps.bas
' Copre TRADING.BONDS.DURATION (7 argomenti) e TRADING.BONDS.PRICESIMPLE (6 argomenti).
' Chiamate qualificate per essere rintracciabili dal controllo di copertura.
'
' FIRME (CommonLib/Numerics/Finance.cs):
'   DURATION(settlement, maturity, coupon, yld, frequency, basis, modified)
'   PRICESIMPLE(settlement, maturity, rate, yld, redemption, frequency)
' Date come SERIALI double: 2026-01-01 = 46023, 2031-01-01 = 47849.

IMPORT TRADING.BONDS

SUB Main()
    DIM s AS DOUBLE
    DIM m AS DOUBLE
    s = 46023
    m = 47849

    DIM d1 = TRADING.BONDS.DURATION(s, m, 0.05, 0.04, 2, 0, FALSE)
    PRINT "DURATION macaulay   = "; d1

    DIM d2 = TRADING.BONDS.DURATION(s, m, 0.05, 0.04, 2, 0, TRUE)
    PRINT "DURATION modificata = "; d2

    DIM p = TRADING.BONDS.PRICESIMPLE(s, m, 0.05, 0.04, 100, 2)
    PRINT "PRICESIMPLE         = "; p

    PRINT "TradingBondsGaps: completato."
END SUB
