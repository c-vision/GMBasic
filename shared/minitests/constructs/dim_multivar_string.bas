' MiniTest: DIM multi-variabile con stringa (`DIM i AS INTEGER, r AS STRING`): la seconda
' variabile deve essere tipata STRING (concatenazione), non numerica.
' EXPECT: r=ab
SUB Main()
  DIM i AS INTEGER, r AS STRING
  i = 1
  r = r + "a"
  r = r + "b"
  PRINT "r="; r
END SUB
