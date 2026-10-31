' ============================================================================
' ChartConfigDemo — grafico OHLC configurato da BASIC (CHART SET/GET/GETS):
' rendering, view window (zoom/pan), autoscale, colori, titolo.
' ============================================================================
' CHART SET   h, "prop", num [, "str"]   ' imposta una proprietà
' CHART GET   h, "prop" INTO var         ' legge una proprietà numerica
' CHART GETS  h, "prop" INTO var$        ' legge una proprietà stringa
'
' Proprietà numeriche: grid, legend, crosshair, autoscale, width/w, height/h,
'   px/x, py/y, xmin, xmax, ymin, ymax, gridcolor, upcolor, downcolor, axiscolor.
' Proprietà stringa: title, xlabel, ylabel.
' Zoom/pan: impostare xmin/xmax/ymin/ymax (spega autoscale); CHART SET h,"autoscale",1
'   per tornare al fit automatico. Il crosshair (se crosshair=1) segue il mouse.
'
' Esecuzione headless: parità 4 backend via MiniTest graphics/chart_config.bas.
' ============================================================================
IMPORT CHART

SUB Main()
  PRINT "=== ChartConfigDemo ==="

  DIM ohlc(24)
  ohlc(0) = 1  : ohlc(1) = 100 : ohlc(2) = 110 : ohlc(3) = 95  : ohlc(4) = 108
  ohlc(5) = 2  : ohlc(6) = 108 : ohlc(7) = 115 : ohlc(8) = 105 : ohlc(9) = 112
  ohlc(10) = 3 : ohlc(11) = 112 : ohlc(12) = 120 : ohlc(13) = 110 : ohlc(14) = 118
  ohlc(15) = 4 : ohlc(16) = 118 : ohlc(17) = 122 : ohlc(18) = 114 : ohlc(19) = 116
  ohlc(20) = 5 : ohlc(21) = 116 : ohlc(22) = 125 : ohlc(23) = 115 : ohlc(24) = 124

  DIM h AS LONG
  h = CHART.CREATE("Candele", "tempo", "prezzo")
  CHART FINANCIAL h, ohlc, "CANDLESTICK", "Asset"

  ' Rendering configurabile.
  CHART SET h, "title", 0, "Asset (configurato)"
  CHART SET h, "ylabel", 0, "prezzo"
  CHART SET h, "grid", 1
  CHART SET h, "legend", 1
  CHART SET h, "crosshair", 1
  CHART SET h, "width", 500
  CHART SET h, "height", 260
  CHART SET h, "upcolor", 10
  CHART SET h, "downcolor", 12

  ' Zoom: view window esplicita (spegne autoscale) sulle prime 3 candele.
  CHART SET h, "xmin", 1
  CHART SET h, "xmax", 3
  CHART SET h, "ymin", 90
  CHART SET h, "ymax", 120
  CHART SHOW h, 0, 0

  ' Lettura di una proprietà.
  DIM ax AS INTEGER
  CHART GET h, "autoscale" INTO ax
  PRINT "autoscale="; ax

  ' Fit: torna all'autoscale.
  CHART SET h, "autoscale", 1
  CHART SHOW h, 0, 0

  CHART CLOSE h
  PRINT "=== fine ==="
END SUB
