' EXPECT: tj=[1,2,3]
IMPORT JSON
SUB Main()
  DIM a = {1, 2, 3}
  PRINT "tj="; JSON.TOJSON(a)
END SUB
