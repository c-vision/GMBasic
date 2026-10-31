' MiniTest: DICT — Count/Keys/Values come MEMBRI (non indici) e d("Count") come chiave.
' Regressione del bug C99Native: d.Count emesso come LEN(lista) e LEN(d.Keys()) con .length.
' EXPECT: dot= 3|keys= 3|values= 3|assigned= 3|idx= 7
IMPORT COLLECTIONS

SUB Main()
  DIM d AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
  DIM x AS DOUBLE
  d("COUNT") = 7
  d("K") = 1
  d("V") = 2
  x = d.Count
  PRINT "dot="; d.Count
  PRINT "keys="; LEN(d.Keys())
  PRINT "values="; LEN(d.Values())
  PRINT "assigned="; x
  PRINT "idx="; d("COUNT")
END SUB
