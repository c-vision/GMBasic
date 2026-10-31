' EXPECT: t=-1|obj={"t":true,"f":false}
' BOOLEAN su chiave JSON: TRUE/FALSE devono restare bool, non -1/0 (C99).
IMPORT JSON
SUB Main()
  DIM o AS JSON = JSON.PARSE("{}")
  o("t") = TRUE
  o("f") = FALSE
  PRINT "t="; o("t")
  PRINT "obj="; JSON.STRINGIFY(o)
END SUB
