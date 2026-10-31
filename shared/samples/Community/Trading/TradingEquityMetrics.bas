' FILE: Trading/TradingEquityMetrics.bas
' =================================================================================================
' ESEMPIO: TRADING.METRICS — metriche azionarie di valutazione (PERatio, DividendYield)
' =================================================================================================
' Copre le metriche di valutazione relative all'equity:
'  - PERatio(price, eps)        → rapporto prezzo/utili (P/E)
'  - DividendYield(dividend, price) → rendimento da dividendo (%)
' =================================================================================================
IMPORT TRADING.METRICS AS TM
IMPORT MATH

SUB Main()
    PRINT "TRADING.METRICS — valutazione azionaria"
    PRINT "P/E  (prezzo 10, EPS 2)      = "; MATH.ROUND(TM.PERatio(10, 2), 4)
    PRINT "Dividend Yield (5 su 100)    = "; MATH.ROUND(TM.DividendYield(5, 100), 4)
END SUB
