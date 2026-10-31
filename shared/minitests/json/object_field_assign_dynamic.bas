' EXPECT: obj={"b":{"x":1},"s":"hi","n":3}|x= 1
' Chiave dinamica: o(k) = JSON.PARSE/string/number (C99 emetteva double sul val).
IMPORT JSON
SUB Main()
  DIM o AS JSON = JSON.PARSE("{}")
  DIM k AS STRING
  k = "b"
  o(k) = JSON.PARSE("{" + CHR$(34) + "x" + CHR$(34) + ":1}")
  o("s") = "hi"
  o("n") = 3
  PRINT "obj="; JSON.STRINGIFY(o)
  PRINT "x="; JSON.EXTRACT(o, "b.x")
END SUB
