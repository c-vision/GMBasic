' ============================================================================
' ChartFinancialDemo — CHART FINANCIAL: serie OHLC / candlestick.
' ============================================================================
' Sintassi:
'     handle = CHART.CREATE("titolo" [, "XLabel", "YLabel"])   ' funzione → handle
'     CHART FINANCIAL handle, ohlc(), "CANDLESTICK"|"OHLC" [, "legenda"]  ' comando
'     CHART SHOW handle, x, y
'     CHART CLOSE handle
'
' Dati: `ohlc()` è un array 1-D di 5 valori per candela, nell'ordine
'   (timestamp, open, high, low, close). Es. 3 candele = 15 valori.
'   Su console testuale il comando è un no-op (grafica-only); in SCREEN n≠0
'   disegna la serie OHLC nel grafico C condiviso.
'
' Esecuzione headless: parità 4 backend via MiniTest graphics/chart_financial.bas.
' ============================================================================
IMPORT CHART

SUB Main()
  PRINT "=== ChartFinancialDemo ==="

  ' 5 candele giornaliere (ts, open, high, low, close).
  DIM ohlc(24)
  ohlc(0) = 1  : ohlc(1) = 100 : ohlc(2) = 110 : ohlc(3) = 95  : ohlc(4) = 108
  ohlc(5) = 2  : ohlc(6) = 108 : ohlc(7) = 115 : ohlc(8) = 105 : ohlc(9) = 112
  ohlc(10) = 3 : ohlc(11) = 112 : ohlc(12) = 120 : ohlc(13) = 110 : ohlc(14) = 118
  ohlc(15) = 4 : ohlc(16) = 118 : ohlc(17) = 122 : ohlc(18) = 114 : ohlc(19) = 116
  ohlc(20) = 5 : ohlc(21) = 116 : ohlc(22) = 125 : ohlc(23) = 115 : ohlc(24) = 124

  DIM h AS LONG
  h = CHART.CREATE("Candele", "tempo", "prezzo")
  CHART FINANCIAL h, ohlc, "CANDLESTICK", "Asset"
  CHART CLOSE h

  PRINT "=== fine ==="
END SUB
