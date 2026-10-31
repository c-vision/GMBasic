' EXPECT: scan3= 3|scan99=-1
SUB Main()
  DIM a(4)
  a(0) = 3
  a(1) = 1
  a(2) = 4
  a(3) = 1
  a(4) = 2
  DIM s = ARRAYS.ASORT(a)
  DIM sc = ARRAYS.ASCAN(s, 3)
  PRINT "scan3="; sc
  DIM sn = ARRAYS.ASCAN(s, 99)
  PRINT "scan99="; sn
END SUB
