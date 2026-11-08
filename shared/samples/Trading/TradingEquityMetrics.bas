' FILE: Trading/TradingEquityMetrics.bas
' =================================================================================================
' ESEMPIO: TRADING.METRICS — metriche azionarie di valutazione (PERatio, DividendYield)
' =================================================================================================
' Copre le metriche di valutazione relative all'equity:
'  - PERatio(price, eps)        → rapporto prezzo/utili (P/E)
'  - DividendYield(dividend, price) → rendimento da dividendo (%)
' =================================================================================================
IMPORT METRICS AS TM
IMPORT FUNDAMENTALS

SUB Main()
    PRINT "METRICS — valutazione azionaria"
    PRINT "P/E  (prezzo 10, EPS 2)      = "; MATH.ROUND(FUNDAMENTALS.PERatio(10, 2), 4)
    PRINT "Dividend Yield (5 su 100)    = "; MATH.ROUND(FUNDAMENTALS.DividendYield(5, 100), 4)
END SUB
