' MiniTest ABS — valore assoluto (numerico, decimale, stringa numerica, BIT/BOOL).
' EXPECT: neg|pos|zero|dec|str|bit|var
SUB Main()
  IF ABS(-5) = 5 THEN PRINT "neg"
  IF ABS(5) = 5 THEN PRINT "pos"
  IF ABS(0) = 0 THEN PRINT "zero"
  IF ABS(-2.75) = 2.75 THEN PRINT "dec"
  IF ABS("-3.5") = 3.5 THEN PRINT "str"
  IF ABS(TRUE) = 1 THEN PRINT "bit"
  DIM b AS BIT
  b = TRUE
  IF ABS(b) = 1 THEN PRINT "var"
END SUB
