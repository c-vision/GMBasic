' EXPECT: A|
' FUNCTION che ritorna null (JSON.EXTRACT di chiave assente) usata come
' argomento di un'altra FUNCTION: PCode OP_RETURN deve pushare NIL, altrimenti
' la CALL_N successiva ha un argomento in meno (SavedStackPointer negativo).
IMPORT JSON
FUNCTION F(j AS JSON) AS STRING
  RETURN JSON.EXTRACT(j, "missing")
END FUNCTION
FUNCTION G(a AS STRING, b AS STRING) AS STRING
  RETURN a + "|" + b
END FUNCTION
SUB Main()
  DIM j AS JSON
  j = JSON.PARSE("{}")
  PRINT G("A", F(j))
END SUB
