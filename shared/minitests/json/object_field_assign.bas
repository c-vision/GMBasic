' EXPECT: obj={"a":1,"b":{"x":[1,2]}}|x= 2
' Assegnazione di un OGGETTO JSON a un campo: o.b = JSON.PARSE(...) (C99Native
' tipava il RHS numerico → cast gmb_val_t→double illegale).
IMPORT JSON
SUB Main()
  DIM o AS JSON = JSON.PARSE("{}")
  o.a = 1
  o.b = JSON.PARSE("{" + CHR$(34) + "x" + CHR$(34) + ":[1,2]}")
  PRINT "obj="; JSON.STRINGIFY(o)
  PRINT "x="; JSON.EXTRACT(o, "b.x[1]")
END SUB
