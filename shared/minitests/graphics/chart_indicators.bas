' Minitest CHART — overlay indicatori (SMA/EMA) + marker trade (buy/sell), nel CORE.
' EXPECT: chartind-ok
IMPORT CHART
SUB Main()
  DIM ohlc(24)
  ohlc(0) = 1  : ohlc(1) = 100 : ohlc(2) = 110 : ohlc(3) = 95  : ohlc(4) = 108
  ohlc(5) = 2  : ohlc(6) = 108 : ohlc(7) = 115 : ohlc(8) = 105 : ohlc(9) = 112
  ohlc(10) = 3 : ohlc(11) = 112 : ohlc(12) = 120 : ohlc(13) = 110 : ohlc(14) = 118
  ohlc(15) = 4 : ohlc(16) = 118 : ohlc(17) = 122 : ohlc(18) = 114 : ohlc(19) = 116
  ohlc(20) = 5 : ohlc(21) = 116 : ohlc(22) = 125 : ohlc(23) = 115 : ohlc(24) = 124

  DIM h AS LONG
  h = CHART.CREATE("Ind", "t", "p")
  CHART FINANCIAL h, ohlc, "CANDLESTICK", "Asset"

  DIM s0 AS INTEGER
  CHART GET h, "series" INTO s0            ' 1 (solo OHLC)

  CHART DRAW h, "sma", 3, 14               ' overlay SMA(3)
  CHART DRAW h, "ema", 2, 10               ' overlay EMA(2)

  DIM s1 AS INTEGER
  CHART GET h, "series" INTO s1            ' 3 (OHLC + SMA + EMA)

  DIM b0 AS INTEGER
  DIM b1 AS INTEGER
  CHART DRAW h, "buy", 2, 105 INTO b0      ' marker acquisto
  CHART DRAW h, "sell", 4, 118 INTO b1     ' marker vendita

  DIM n AS INTEGER
  CHART DRAW h, "count" INTO n             ' 2 marker

  CHART CLOSE h
  IF s0 = 1 AND s1 = 3 AND n = 2 AND b0 = 1 AND b1 = 2 THEN PRINT "chartind-ok" ELSE PRINT "chartind-no " + STR(s0) + "|" + STR(s1) + "|" + STR(n) + "|" + STR(b0) + "|" + STR(b1)
END SUB
