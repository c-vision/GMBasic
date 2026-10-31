' MiniTest builtin numerici con ARGOMENTO STRINGA numerica (coercizione).
' EXPECT: abs|sqr|exp|log|sin|cos
SUB Main()
  IF ABS("-3.5") = 3.5 THEN PRINT "abs"
  IF SQR("4") = 2 THEN PRINT "sqr"
  IF EXP("0") = 1 THEN PRINT "exp"
  IF LOG("1") = 0 THEN PRINT "log"
  IF SIN("0") = 0 THEN PRINT "sin"
  IF COS("0") = 1 THEN PRINT "cos"
END SUB
