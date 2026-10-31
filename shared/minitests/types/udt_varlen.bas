' udt_varlen.bas — TYPE accetta campi a lunghezza variabile (STRING/LIST/DICT).
' (livello dichiarazione: tutti i backend devono compilare ed eseguire)
TYPE Box
  name AS STRING * 16
  tags AS LIST OF DOUBLE
  meta AS DICT OF STRING, DOUBLE
  note AS STRING
END TYPE
SUB Main()
  DIM b AS Box
  b.name = "hello"
  PRINT b.name
  PRINT "udt varlen ok"
END SUB
