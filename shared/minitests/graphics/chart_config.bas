' Minitest CHART SET/GET/GETS — configurazione grafico da BASIC (rendering + view window zoom/pan).
' EXPECT: chartcfg-ok
IMPORT CHART
SUB Main()
  DIM h AS LONG
  DIM g AS INTEGER
  DIM ax AS INTEGER
  DIM w AS INTEGER
  DIM s AS STRING

  h = CHART.CREATE("Cfg", "x", "y")

  ' default: griglia ON, autoscale ON, width 400
  CHART GET h, "grid" INTO g
  CHART GET h, "autoscale" INTO ax
  CHART GET h, "width" INTO w
  DIM d_grid AS INTEGER
  d_grid = g

  ' rendering OFF + view window (zoom/pan: autoscale OFF) + panel size
  CHART SET h, "grid", 0
  CHART SET h, "xmin", 1
  CHART SET h, "xmax", 50
  CHART SET h, "ymin", 0
  CHART SET h, "ymax", 100
  CHART SET h, "width", 200
  DIM g2 AS INTEGER
  DIM a2 AS INTEGER
  DIM w2 AS INTEGER
  CHART GET h, "grid" INTO g2
  CHART GET h, "autoscale" INTO a2
  CHART GET h, "width" INTO w2

  ' titolo (stringa)
  CHART SET h, "title", 0, "Nuovo"
  CHART GETS h, "title" INTO s

  CHART CLOSE h

  IF d_grid = 1 AND ax = 1 AND w = 400 AND g2 = 0 AND a2 = 0 AND w2 = 200 AND s = "Nuovo" THEN
    PRINT "chartcfg-ok"
  ELSE
    PRINT "chartcfg-no " + STR(d_grid) + "|" + STR(ax) + "|" + STR(w) + "|" + STR(g2) + "|" + STR(a2) + "|" + STR(w2) + "|" + s
  END IF
END SUB
