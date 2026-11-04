' EXPECT: csv= 9
SUB Main()
  DIM m = MATRIX.LOADCSV("mt_loadcsv_fixture.csv")
  PRINT "csv="; MATH.ROUND(MATRIX.GETVALUE(m,2,2),6)
END SUB