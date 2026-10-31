' Minitest CHART DRAW — oggetti di disegno (line/hline/vline/rect/fib/move/remove/clear/count/hit).
' EXPECT: chartdraw-ok
IMPORT CHART
SUB Main()
  DIM h AS LONG
  DIM id1 AS INTEGER
  DIM id2 AS INTEGER
  DIM id3 AS INTEGER
  DIM id4 AS INTEGER
  DIM n1 AS INTEGER
  DIM n2 AS INTEGER
  DIM hit AS INTEGER

  h = CHART.CREATE("Draw", "x", "y")
  CHART DRAW h, "line", 1, 10, 5, 20 INTO id1
  CHART DRAW h, "hline", 15 INTO id2
  CHART DRAW h, "vline", 3 INTO id3
  CHART DRAW h, "rect", 2, 12, 4, 18 INTO id4
  CHART DRAW h, "count" INTO n1
  CHART DRAW h, "move", id1, 1, 10, 6, 25
  CHART DRAW h, "remove", id2
  CHART DRAW h, "count" INTO n2
  CHART DRAW h, "hit", 1, 10 INTO hit
  CHART DRAW h, "clear"
  DIM n3 AS INTEGER
  CHART DRAW h, "count" INTO n3
  CHART CLOSE h

  IF id1 = 1 AND id2 = 2 AND id3 = 3 AND id4 = 4 AND n1 = 4 AND n2 = 3 AND n3 = 0 AND hit >= 1 THEN
    PRINT "chartdraw-ok"
  ELSE
    PRINT "chartdraw-no " + STR(id1) + "|" + STR(id2) + "|" + STR(id3) + "|" + STR(id4) + "|" + STR(n1) + "|" + STR(n2) + "|" + STR(n3) + "|" + STR(hit)
  END IF
END SUB
