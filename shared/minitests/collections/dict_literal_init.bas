' EXPECT: dict_ok
' Dizionari con letterale `{ "chiave": valore, ... }`: tipizzato (AS DICT OF K,V) e inferito
' (DIM d = { ... }). Lettura/scrittura per chiave.
SUB Main()
  DIM typed AS DICT OF STRING, INTEGER = {"uno": 1, "due": 2}
  DIM inferred = {"a": 10, "b": 20}
  DIM s AS DICT OF STRING, STRING = {"x": "hello", "y": "world"}
  inferred("c") = 30
  DIM ok AS INTEGER = 1
  IF typed("uno") <> 1 OR typed("due") <> 2 THEN ok = 0
  IF inferred("a") <> 10 OR inferred("b") <> 20 OR inferred("c") <> 30 THEN ok = 0
  IF s("x") <> "hello" OR s("y") <> "world" THEN ok = 0
  IF ok = 1 THEN
    PRINT "dict_ok"
  ELSE
    PRINT "dict_FAIL"
  END IF
END SUB
