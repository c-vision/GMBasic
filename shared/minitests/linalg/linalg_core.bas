' MiniTest — namespace LINALG (algebra lineare su ARRAY via FFI, gmbmath).
' La matrice A è 2×2 piena, in un array 1-D row-major (il contratto FFI di LINALG
' è "buffer piatto + dimensioni esplicite": una matrice DIM a(r,c) NON è portabile
' come parametro FFI perché le gambe C passano le lunghezze per dimensione):
'   A = [ 2  1 ]
'       [ 1  3 ]      -> a = {2, 1, 1, 3}
' det=5, rango=2; norme: Frobenius=sqrt(15), 1=max(3,4)=4, infinito=max(3,4)=4;
' valori singolari 3.618034 e 1.381966 (A simmetrica SPD) -> norma 2=3.618034,
' cond=2.618034, nucleare=5. Solve A x = [3,5] -> [0.8, 1.4]; inversa 1/5·[3 -1; -1 2].
' Triangolare inferiore L=[[2,0],[1,3]] con b=[4,7]: x=[2,5/3].
' EXPECT: det= 5|rank= 2|fro= 3.872983|one= 4|inf= 4|norm= 3.618034|cond= 2.618034|nuc= 5|x0= 0.8  x1= 1.4|rs= 0|i00= 0.6  i01=-0.2|i10=-0.2  i11= 0.4|rt= 0  t0= 2  t1= 1.666667
IMPORT LINALG

SUB Main()
  DIM a(3) AS DOUBLE
  a(0) = 2 : a(1) = 1
  a(2) = 1 : a(3) = 3

  PRINT "det="; LINALG.DETERMINANT(a, 2)
  PRINT "rank="; LINALG.RANK(a, 2, 2)
  PRINT "fro="; ROUND(LINALG.FROBENIUSNORM(a, 2, 2), 6)
  PRINT "one="; ROUND(LINALG.ONENORM(a, 2, 2), 6)
  PRINT "inf="; ROUND(LINALG.INFINITYNORM(a, 2, 2), 6)
  PRINT "norm="; ROUND(LINALG.NORM(a, 2, 2), 6)
  PRINT "cond="; ROUND(LINALG.COND(a, 2, 2), 6)
  PRINT "nuc="; ROUND(LINALG.NUCLEARNORM(a, 2, 2), 6)

  DIM b(1) AS DOUBLE
  DIM x(1) AS DOUBLE
  b(0) = 3 : b(1) = 5
  DIM rs AS LONG
  rs = LINALG.SOLVE(a, 2, b, x)
  PRINT "x0="; ROUND(x(0), 6); " x1="; ROUND(x(1), 6)
  PRINT "rs="; rs

  DIM inv(3) AS DOUBLE
  DIM ri AS LONG
  ri = LINALG.INVERSE(a, 2, inv)
  PRINT "i00="; ROUND(inv(0), 6); " i01="; ROUND(inv(1), 6)
  PRINT "i10="; ROUND(inv(2), 6); " i11="; ROUND(inv(3), 6)

  ' triangolare inferiore L = [[2,0],[1,3]], b = [4,7] -> x = [2, 5/3]
  DIM L(3) AS DOUBLE
  L(0) = 2 : L(1) = 0
  L(2) = 1 : L(3) = 3
  DIM bt(1) AS DOUBLE
  bt(0) = 4 : bt(1) = 7
  DIM xt(1) AS DOUBLE
  DIM rt AS LONG
  rt = LINALG.SOLVETRIANGULAR(L, 2, bt, 1, xt)
  PRINT "rt="; rt; " t0="; ROUND(xt(0), 6); " t1="; ROUND(xt(1), 6)
END SUB
