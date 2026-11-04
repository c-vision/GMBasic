' EXPECT: json= 1
SUB Main()
  DIM m = MATRIX.LOADJSON("mt_loadjson_fixture.json")
  PRINT "json="; MATH.ROUND(MATRIX.GETVALUE(m,0,0),6)
END SUB