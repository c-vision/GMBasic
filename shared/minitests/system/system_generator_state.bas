' MiniTest GENERATOR: due istanze indipendenti, stato locale, argomenti multipli (valori numerici).
' EXPECT: 10 20|7|30|stop
IMPORT SYSTEM

GENERATOR Multipli(fattore AS LONG, n AS LONG) AS LONG
  DIM i AS LONG
  FOR i = 1 TO n
    YIELD fattore * i
  NEXT i
END GENERATOR

SUB Main()
  DIM a = Multipli(10, 3)
  DIM b = Multipli(7, 1)
  DIM s AS STRING
  DIM r AS STRING
  r = LTRIM$(STR$(NEXT(a))) + " "
  r = r + LTRIM$(STR$(NEXT(a))) + "|"
  r = r + LTRIM$(STR$(NEXT(b))) + "|"
  r = r + LTRIM$(STR$(NEXT(a))) + "|"
  s = "ok"
  TRY
    s = LTRIM$(STR$(NEXT(b)))
  CATCH e AS ERR
    s = "stop"
  END TRY
  PRINT r + s
END SUB
