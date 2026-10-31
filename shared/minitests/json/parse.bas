' EXPECT: name=Ada
IMPORT JSON
SUB Main()
  DIM q$ = CHR(34)
  DIM o = JSON.PARSE("{" + q$ + "name" + q$ + ": " + q$ + "Ada" + q$ + "}")
  PRINT "name="; JSON.EXTRACT(o, "name")
END SUB
