' EXPECT: long= 100 , 200 , 300|double= 1.5 , 3.5|str=abc
SUB Main()
  DIM L(2) AS LONG
  FOR I = 0 TO 2 : L[I] = (I + 1) * 100 : NEXT I
  PRINT "long="; L[0]; ","; L[1]; ","; L[2]
  DIM D(2) AS DOUBLE
  D[0] = 1.5 : D[1] = 2.5 : D[2] = 3.5
  PRINT "double="; D[0]; ","; D[2]
  DIM S$(2)
  S$[0] = "a" : S$[1] = "b" : S$[2] = "c"
  PRINT "str="; S$[0]; S$[1]; S$[2]
END SUB
