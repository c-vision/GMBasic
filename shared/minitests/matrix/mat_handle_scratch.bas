' EXPECT: s= 1000|rows= 2|cols= 2
' Stage C backend C: il ponte descrittore→handle usa slot SCRATCH riusati. Un ciclo lungo
' su un valore matrice non deve esaurire il registry (256 slot fissi) né far crescere la
' memoria: 500 operazioni MATRIX.ROWCOUNT su un descrittore.
SUB Main()
  DIM m = MATRIX.CREATE(2, 2)
  DIM i AS LONG
  DIM s AS LONG
  FOR i = 1 TO 500
    s = s + MATRIX.ROWCOUNT(m)
  NEXT i
  PRINT "s="; s
  PRINT "rows="; MATRIX.ROWCOUNT(m)
  PRINT "cols="; MATRIX.COLUMNCOUNT(m)
END SUB
