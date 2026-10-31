' EXPECT: T=["a\tb"]|A=["a\u0027b\u0060c\u003Cd\u003Ee\u0026f"]|C=["\u0001\u001F\u007F"]|K=[{"k\t":1}]
' Escape JSON = System.Text.Json (JavaScriptEncoder.Default): control TAB -> \t,
' ' ` < > & -> \uXXXX, C0/DEL -> \uXXXX, hex MAIUSCOLO; anche nelle CHIAVI dei dict.
' Il runtime C li lasciava grezzi (JSON invalido; parità rotta con IL).
IMPORT JSON
SUB Main()
  PRINT "T=["; JSON.TOJSON("a" & CHR(9) & "b"); "]"
  PRINT "A=["; JSON.TOJSON("a'b`c<d>e&f"); "]"
  PRINT "C=["; JSON.TOJSON(CHR(1) & CHR(31) & CHR(127)); "]"
  DIM d AS DICT OF STRING, INTEGER
  d = NEW DICT OF STRING, INTEGER
  d("k" & CHR(9)) = 1
  PRINT "K=["; JSON.TOJSON(d); "]"
END SUB
