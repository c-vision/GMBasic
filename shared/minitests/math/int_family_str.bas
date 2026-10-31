' MiniTest famiglia intera (INT/FIX/FLOOR/TRUNC/CEILING) con ARGOMENTO STRINGA.
' EXPECT: int|fix|floor|trunc|ceil
SUB Main()
  IF INT("3.9") = 3 THEN PRINT "int"
  IF FIX("-3.9") = -3 THEN PRINT "fix"
  IF FLOOR("3.9") = 3 THEN PRINT "floor"
  IF TRUNC("-3.9") = -3 THEN PRINT "trunc"
  IF CEILING("3.1") = 4 THEN PRINT "ceil"
END SUB
