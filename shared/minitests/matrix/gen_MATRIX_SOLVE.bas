' EXPECT: x0= 1
' NOTA: SOLVE su PCode richiede vector handle (Long), non raw array — skip PCode. IL/VMC OK.
SUB Main()
  DIM a = MATRIX.CREATE(2,2)
  a = MATRIX.SETVALUE(a,0,0,2) : a = MATRIX.SETVALUE(a,0,1,1)
  a = MATRIX.SETVALUE(a,1,0,1) : a = MATRIX.SETVALUE(a,1,1,2)
  DIM b(1)
  b(0)=4 : b(1)=5
  DIM xs() = LINALG.SOLVE(a, b)
  PRINT "x0="; MATH.ROUND(xs(0),6)
END SUB