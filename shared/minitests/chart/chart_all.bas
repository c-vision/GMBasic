' MiniTest NAMESPACE CHART — unica funzione della superficie: CREATE.
' Headless: il chart engine non ha display, quindi si verifica che CREATE allochi un
' handle valido (> 0) e che la distruzione (CHART CLOSE) sia accettata.
' EXPECT: created=-1|closed=ok
IMPORT CHART

SUB Main()
  DIM h AS LONG
  h = CHART.CREATE("MiniTest", "x", "y")
  IF h > 0 THEN PRINT "created=-1" ELSE PRINT "created=0"
  CHART CLOSE h
  PRINT "closed=ok"
END SUB
