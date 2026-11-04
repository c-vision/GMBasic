' EXPECT: NM= 1 , 1|BF= 0.999997 , 0.999995|LB= 0.999997 , 0.999994|PW= 0.249913 , 0.040301|CG= 0 , 0
' Port FFI di optimize/multidim.bas.
IMPORT OPTIMIZE
SUB Main()
  DIM Rosen = FN(v() AS DOUBLE) AS DOUBLE => (1 - v(0)) ^ 2 + 100 * (v(1) - v(0) ^ 2) ^ 2
  DIM G(1) AS DOUBLE = {0.0, 0.0}
  DIM o(1) AS DOUBLE
  CALL OPTIMIZE.NELDERMEAD(Rosen, G, o)
  PRINT "NM="; MATH.ROUND(o(0), 6); ","; MATH.ROUND(o(1), 6)
  CALL OPTIMIZE.BFGS(Rosen, G, o)
  PRINT "BF="; MATH.ROUND(o(0), 6); ","; MATH.ROUND(o(1), 6)
  CALL OPTIMIZE.LBFGS(Rosen, G, o)
  PRINT "LB="; MATH.ROUND(o(0), 6); ","; MATH.ROUND(o(1), 6)
  CALL OPTIMIZE.POWELL(Rosen, G, o)
  PRINT "PW="; MATH.ROUND(o(0), 6); ","; MATH.ROUND(o(1), 6)
  DIM Quad = FN(v() AS DOUBLE) AS DOUBLE => v(0) ^ 2 + v(1) ^ 2
  DIM QG(1) AS DOUBLE = {1.0, 1.0}
  CALL OPTIMIZE.CONJUGATEGRADIENT(Quad, QG, o)
  PRINT "CG="; MATH.ROUND(o(0), 6); ","; MATH.ROUND(o(1), 6)
END SUB
