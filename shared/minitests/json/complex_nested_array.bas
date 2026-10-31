' EXPECT: obj={"a":{"b":1,"c":{"x":1}},"arr":[9,2],"k":{"b":1,"c":{"x":1}}}
' Casi complessi: campo annidato, oggetto a campo, array JSON con indice, chiave dinamica.
IMPORT JSON
SUB Main()
  DIM o AS JSON = JSON.PARSE("{}")
  DIM a AS JSON = JSON.PARSE("{}")
  o.a = a
  o.a.b = 1
  o.a.c = JSON.PARSE("{" + CHR$(34) + "x" + CHR$(34) + ":1}")
  o.arr = JSON.PARSE("[1,2]")
  o.arr[0] = 9
  o("k") = a
  PRINT "obj="; JSON.STRINGIFY(o)
END SUB
