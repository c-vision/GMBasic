' Minitest CHART SET/GET "candlewidth" — larghezza corpo candela regolabile (0..1).
' EXPECT: chartcandle-ok
IMPORT CHART
SUB Main()
  DIM h AS LONG
  DIM cw0 AS DOUBLE
  DIM cw1 AS DOUBLE
  DIM cw2 AS DOUBLE

  h = CHART.CREATE("C", "x", "y")

  ' default (0.64) e set/get
  CHART GET h, "candlewidth" INTO cw0
  CHART SET h, "candlewidth", 0.25
  CHART GET h, "candlewidth" INTO cw1
  ' clamping: >1 viene limitato a 1
  CHART SET h, "candlewidth", 5
  CHART GET h, "candlewidth" INTO cw2
  CHART CLOSE h

  IF ABS(cw0 - 0.64) < 0.0001 AND ABS(cw1 - 0.25) < 0.0001 AND ABS(cw2 - 1.0) < 0.0001 THEN
    PRINT "chartcandle-ok"
  ELSE
    PRINT "chartcandle-no " + STR(cw0) + "|" + STR(cw1) + "|" + STR(cw2)
  END IF
END SUB
