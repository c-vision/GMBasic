' MiniTest: STATS.NORMALTEST — D'Agostino-Pearson, richiede n>=20
' (contratto ValidateSample: meno di 20 solleva). 1..20: skewZ=1.040349 (y=1),
' kurtZ=-1.70581; k2=z1^2+z2^2=3.992116 (scipy 1.18).
' EXPECT: k2= 3.992116|p= 0.13587
IMPORT STATS
SUB Main()
  DIM z = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0, 17.0, 18.0, 19.0, 20.0}
  DIM r = STATS.NORMALTEST(z)
  PRINT "k2="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB
