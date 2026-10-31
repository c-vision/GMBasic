' Minitest CHART FINANCIAL — serie OHLC (5 valori/candela: timestamp,open,high,low,close) — 4 backend.
IMPORT CHART
SUB Main()
  DIM ohlc(14)
  ' 3 candele: ts, open, high, low, close
  ohlc(0) = 1  : ohlc(1) = 10 : ohlc(2) = 12 : ohlc(3) = 9  : ohlc(4) = 11
  ohlc(5) = 2  : ohlc(6) = 11 : ohlc(7) = 13 : ohlc(8) = 10 : ohlc(9) = 12
  ohlc(10) = 3 : ohlc(11) = 12 : ohlc(12) = 14 : ohlc(13) = 11 : ohlc(14) = 13
  DIM h AS LONG
  h = CHART.CREATE("Test", "t", "prezzo")
  CHART FINANCIAL h, ohlc, "CANDLESTICK", "OHLC"
  CHART SHOW h, 0, 0
  CHART CLOSE h
  PRINT "OK-chart-financial"
END SUB
