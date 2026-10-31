' EXPECT: join=3-1-2|clone0= 3|src0= 9
SUB Main()
  DIM a(2)
  a(0) = 3
  a(1) = 1
  a(2) = 2
  DIM j = ARRAYS.AJOIN(a, "-")
  PRINT "join="; j
  DIM c = ARRAYS.ACLONE(a)
  a(0) = 9
  PRINT "clone0="; c(0)
  PRINT "src0="; a(0)
END SUB
