' ============================================================================
' ChartDrawDemo — oggetti di disegno su grafico (CHART DRAW), config-driven da BASIC.
' ============================================================================
' CHART DRAW h, "op"[, args...] [INTO id]
'   op: "line"  x1,y1,x2,y2[,color]      trendline (coord. DATI)
'       "hline" y[,color]                linea orizzontale
'       "vline" x[,color]                linea verticale
'       "rect"  x1,y1,x2,y2[,color]      rettangolo
'       "fib"   x1,y1,x2,y2[,color]      ritracciamento di Fibonacci (7 livelli)
'       "move"  id,x1,y1,x2,y2           sposta/trascina un oggetto
'       "remove" id                      elimina
'       "clear"                          svuota
'       "count"                          numero di oggetti (INTO)
'       "hit"   x,y                      id dell'oggetto più vicino al punto (INTO)
'
' Esecuzione headless: parità 4 backend via MiniTest graphics/chart_draw.bas.
' ============================================================================
IMPORT CHART

SUB Main()
  PRINT "=== ChartDrawDemo ==="

  DIM ohlc(24)
  ohlc(0) = 1  : ohlc(1) = 100 : ohlc(2) = 110 : ohlc(3) = 95  : ohlc(4) = 108
  ohlc(5) = 2  : ohlc(6) = 108 : ohlc(7) = 115 : ohlc(8) = 105 : ohlc(9) = 112
  ohlc(10) = 3 : ohlc(11) = 112 : ohlc(12) = 120 : ohlc(13) = 110 : ohlc(14) = 118
  ohlc(15) = 4 : ohlc(16) = 118 : ohlc(17) = 122 : ohlc(18) = 114 : ohlc(19) = 116
  ohlc(20) = 5 : ohlc(21) = 116 : ohlc(22) = 125 : ohlc(23) = 115 : ohlc(24) = 124

  DIM h AS LONG
  h = CHART.CREATE("Disegni", "tempo", "prezzo")
  CHART FINANCIAL h, ohlc, "CANDLESTICK", "Asset"

  DIM tl AS INTEGER
  DIM r AS INTEGER
  DIM fx AS INTEGER
  CHART DRAW h, "line", 1, 100, 5, 125 INTO tl
  CHART DRAW h, "hline", 110
  CHART DRAW h, "vline", 3
  CHART DRAW h, "rect", 2, 105, 4, 118 INTO r
  CHART DRAW h, "fib", 1, 95, 5, 124 INTO fx

  DIM n AS INTEGER
  CHART DRAW h, "count" INTO n
  PRINT "oggetti="; n

  ' trascina la trendline e rimuovi la Fibonacci
  CHART DRAW h, "move", tl, 1, 100, 5, 130
  CHART DRAW h, "remove", fx

  ' chi è vicino al punto (4, 116)? (in coord. dati)
  DIM hh AS INTEGER
  CHART DRAW h, "hit", 4, 116 INTO hh
  PRINT "hit="; hh

  ' Selezione/trascinamento in coordinate PIXEL (view window + pannello espliciti).
  ' In un'app interattiva mx/my vengono dal mouse; qui valori fissi per dimostrazione.
  CHART SET h, "autoscale", 0
  CHART SET h, "xmin", 1
  CHART SET h, "xmax", 5
  CHART SET h, "ymin", 95
  CHART SET h, "ymax", 125
  CHART SET h, "width", 110
  CHART SET h, "height", 110
  DIM sel AS INTEGER
  CHART DRAW h, "pick", 55, 36 INTO sel
  PRINT "pick="; sel
  CHART DRAW h, "drag", sel, 40, 60

  CHART SHOW h, 0, 0
  CHART CLOSE h
  PRINT "=== fine ==="
END SUB
