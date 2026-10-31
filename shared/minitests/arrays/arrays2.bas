' EXPECT: map= 6 , 2 , 8 , 2 , 4|cnt= 2|bs= 2
SUB Main()
  DIM a(4)
  a(0) = 3
  a(1) = 1
  a(2) = 4
  a(3) = 1
  a(4) = 2
  DIM d = ARRAYS.AMAP(a, FN(x) => x * 2)
  PRINT "map="; d(0); ","; d(1); ","; d(2); ","; d(3); ","; d(4)
  DIM c = ARRAYS.ACOUNT(a, FN(x) => x > 2)
  PRINT "cnt="; c
  DIM s = ARRAYS.ASORT(a)
  DIM b = ARRAYS.ABINARYSEARCH(s, 2)
  PRINT "bs="; b
END SUB
