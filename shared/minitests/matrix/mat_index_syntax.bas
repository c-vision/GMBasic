' EXPECT: a= 1 , 2|b= 3 , 4|sum= 10|g= 7|v= 9
' Sintassi immediata delle matrici 2x2: DIM m(r, c) AS DOUBLE, scrittura e lettura
' per indice. MATRIX.GETVALUE/SETVALUE restano il ponte storico e devono vedere la
' stessa matrice (descrittore <-> lista flat) in ENTRAMBE le direzioni.
SUB Main()
  DIM m(1, 1) AS DOUBLE
  m(0, 0) = 1
  m(0, 1) = 2
  m(1, 0) = 3
  m(1, 1) = 4
  PRINT "a="; m(0, 0); ","; m(0, 1)
  PRINT "b="; m(1, 0); ","; m(1, 1)
  PRINT "sum="; m(0, 0) + m(0, 1) + m(1, 0) + m(1, 1)
  ' indice -> GETVALUE (il ponte deve leggere lo stesso valore)
  m = MATRIX.SETVALUE(m, 0, 0, 7)
  PRINT "g="; MATRIX.GETVALUE(m, 0, 0)
  ' SETVALUE -> indice (la scrittura via ponte deve essere visibile per indice)
  m = MATRIX.SETVALUE(m, 1, 1, 9)
  PRINT "v="; m(1, 1)
END SUB
