' MiniTest: una variabile che INFERISCE il tipo da un inizializzatore DATETIME
' (`DIM u = NOW()`) si comporta come una data: LEN 19, PRINT formattato, JSON ISO.
' Il C99 la trattava come numero (LEN 17, seriale in JSON) mentre IL/PCode/VM-C
' usavano il tipo data. Include il caso esplicito `DIM t AS DATETIME`.
' EXPECT: u= 19|t= 19|iso="2020-01-02T00:00:00"|v=02/01/2020 00:00:00
IMPORT JSON

SUB Main()
  DIM u = NOW()
  DIM t AS DATETIME = DATEVALUE("2020-01-02")
  DIM v = DATEVALUE("2020-01-02")
  PRINT "u="; LEN(u)
  PRINT "t="; LEN(t)
  PRINT "iso="; JSON.STRINGIFY(v)
  PRINT "v="; v
END SUB
