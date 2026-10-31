' Minitest CHART zoom/pan — view window (nel core), op "zoom"/"pan" da BASIC.
' EXPECT: chartzoom-ok
IMPORT CHART
SUB Main()
  DIM h AS LONG
  h = CHART.CREATE("Z", "x", "y")
  CHART SET h, "autoscale", 0
  CHART SET h, "xmin", 0
  CHART SET h, "xmax", 10
  CHART SET h, "ymin", 0
  CHART SET h, "ymax", 10

  ' zoom in (factor 0.5) attorno al centro view (5,5) → range 0..10 → 2.5..7.5
  CHART DRAW h, "zoom", 0.5
  DIM x0 AS DOUBLE
  DIM x1 AS DOUBLE
  DIM y0 AS DOUBLE
  DIM y1 AS DOUBLE
  CHART GET h, "xmin" INTO x0
  CHART GET h, "xmax" INTO x1
  CHART GET h, "ymin" INTO y0
  CHART GET h, "ymax" INTO y1

  ' pan +1 in x → xmin 3.5
  CHART DRAW h, "pan", 1, 0
  DIM x2 AS DOUBLE
  CHART GET h, "xmin" INTO x2

  CHART CLOSE h
  IF x0 = 2.5 AND x1 = 7.5 AND y0 = 2.5 AND y1 = 7.5 AND x2 = 3.5 THEN PRINT "chartzoom-ok" ELSE PRINT "chartzoom-no " + STR(x0) + "|" + STR(x1) + "|" + STR(y0) + "|" + STR(y1) + "|" + STR(x2)
END SUB
