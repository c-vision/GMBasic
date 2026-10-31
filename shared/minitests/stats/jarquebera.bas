' MiniTest: STATS.JARQUEBERA — formula JB=n/6*(S^2+(K-3)^2) con momenti distorti:
' S=0, K-3=-1.206015 -> JB=4.848241, p=exp(-JB/2)=0.088556 (scipy=1.21206/0.545512,
' usa momenti corretti per bias).
' EXPECT: jb= 4.848241|p= 0.088556
IMPORT STATS
IMPORT MATH
SUB Main()
  DIM z = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0, 17.0, 18.0, 19.0, 20.0}
  DIM r = STATS.JARQUEBERA(z)
  PRINT "jb="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB
