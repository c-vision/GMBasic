' Minitest CHART DRAW pick/drag — selezione e trascinamento oggetti in coordinate PIXEL.
' EXPECT: chartpick-ok
IMPORT CHART
SUB Main()
  DIM h AS LONG
  DIM idl AS INTEGER
  DIM pk1 AS INTEGER
  DIM pk2 AS INTEGER

  h = CHART.CREATE("Pick", "x", "y")
  ' view window esplicita + pannello 110x110 in (0,0): mapping dati→pixel deterministico.
  CHART SET h, "autoscale", 0
  CHART SET h, "xmin", 0
  CHART SET h, "xmax", 10
  CHART SET h, "ymin", 0
  CHART SET h, "ymax", 10
  CHART SET h, "width", 110
  CHART SET h, "height", 110

  ' linea (0,0)-(10,10): il punto dati (5,5) cade ~ al pixel (55,54).
  CHART DRAW h, "line", 0, 0, 10, 10 INTO idl
  CHART DRAW h, "pick", 55, 54 INTO pk1

  ' trascina la linea verso il pixel (20,20) e ri-seleziona lì.
  CHART DRAW h, "drag", idl, 20, 20
  CHART DRAW h, "pick", 20, 20 INTO pk2

  CHART CLOSE h
  IF idl = 1 AND pk1 = 1 AND pk2 = 1 THEN PRINT "chartpick-ok" ELSE PRINT "chartpick-no " + STR(idl) + "|" + STR(pk1) + "|" + STR(pk2)
END SUB
