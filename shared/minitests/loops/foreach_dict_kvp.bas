' MiniTest: FOR EACH su DICT con UNA variabile — l'elemento è la COPPIA
' KeyValuePair, stampata `[[Key, k], [Value, v]]` (canone 2026-09-30).
' EXPECT: [[Key, a], [Value, 1]]
SUB Main()
  DIM d AS DICT OF STRING, INTEGER = {"a": 1}
  FOR EACH e IN d
    PRINT e
  NEXT
END SUB
