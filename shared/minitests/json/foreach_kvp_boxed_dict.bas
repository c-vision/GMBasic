' MiniTest: `FOR EACH e IN <dict boxed>` (da JSON.Parse) → e è un KeyValuePair con
' e.Value del tipo corretto (numerico per valori JSON numerici). Regressione per bug
' C99Native-Foreach-KVP-Dict-Boxed-Value-Stringa (C99Native).
' EXPECT: pairs= 6|kvp= 6
IMPORT JSON

SUB Main()
  DIM d = JSON.Parse("{ ""a"": 1, ""b"": 2, ""c"": 3 }")
  DIM vs = 0
  FOR EACH k, v IN d
    vs = vs + v
  NEXT
  PRINT "pairs="; vs
  DIM kv = 0
  FOR EACH e IN d
    kv = kv + e.Value
  NEXT
  PRINT "kvp="; kv
END SUB
