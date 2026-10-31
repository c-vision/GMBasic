' EXPECT: map= 1 , 4 , 9 , 16|filter= 2 , 4|reduce= 10
' AMAP/AFILTER/AREDUCE con lambda a PARAMETRI TIPATI (FN(n AS INTEGER) ...): verifica la
' parita' 4 backend della forma tipata (ABI callback) corretta il 2026-09-24.
IMPORT ARRAYS
SUB Main()
  DIM a = {1, 2, 3, 4}
  DIM m = ARRAYS.AMAP(a, FN(n AS INTEGER) AS INTEGER => n * n)
  PRINT "map="; m(0); ","; m(1); ","; m(2); ","; m(3)
  DIM f = ARRAYS.AFILTER(a, FN(n AS INTEGER) AS INTEGER => (n MOD 2) = 0)
  PRINT "filter="; f(0); ","; f(1)
  DIM r = ARRAYS.AREDUCE(a, FN(acc AS INTEGER, x AS INTEGER) AS INTEGER => acc + x)
  PRINT "reduce="; r
END SUB
