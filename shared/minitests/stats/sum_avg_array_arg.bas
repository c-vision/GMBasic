' SUM/AVG/MIN/MAX con un ARRAY come argomento e SUM con argomenti numerici in PRINT (IL: l'array veniva trattato come elemento singolo →
' AVG(d) = 2.4E-314, e `PRINT SUM(..)` generava IL non valido perche' SUM mancava nell'inferenza del tipo).
' EXPECT: sum= 24|avg= 4.8|min= 1|max= 9|s3= 6
SUB Main()
  DIM d(4) AS DOUBLE = {5, 2, 9, 1, 7}
  PRINT "sum="; SUM(d)
  PRINT "avg="; AVG(d)
  PRINT "min="; MIN(d)
  PRINT "max="; MAX(d)
  PRINT "s3="; SUM(1, 2, 3)
END SUB
