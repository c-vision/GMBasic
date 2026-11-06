' NEURAL (FFI) — scaler adattivo (gn_norm), gather/block (gn_ops), apply/absmax (gn_core).
' EXPECT: n= 7201 : 0 , 0 : 2 : 0 |ap= 0 =1.0,1.0|dn= 0 =3.0,4.0|zs= 0 =0.7071,0.7071|st= 0 =3.0,4.0,2.0,3.0,1.4142,1.4142|nf= 0 |ga= 0 =5.0,6.0,1.0,2.0|sl= 0 =3.0,4.0,5.0,6.0|cb= 0 =5.0,6.0,8.0,9.0|ap2= 0 =2.0,5.0,10.0|mx=5.0|f=0
IMPORT NEURAL

' Funzione applicata elemento per elemento (MATAPPLY).
FUNCTION QuadPlus1(x AS DOUBLE) AS DOUBLE
  RETURN x * x + 1
END FUNCTION

SUB Main()
  DIM o1(1) AS DOUBLE, o2(1) AS DOUBLE, v(1) AS DOUBLE, o(1) AS DOUBLE, st(5) AS DOUBLE
  o1(0)=1 : o1(1)=2
  o2(0)=3 : o2(1)=4
  nh = NEURAL.NORMINIT(2)
  PRINT "n="; nh; ":"; NEURAL.NORMOBSERVE(nh, o1); ","; NEURAL.NORMOBSERVE(nh, o2);
  PRINT ":"; NEURAL.NORMCOUNT(nh); ":"; NEURAL.NORMFINALIZE(nh, 1.0);
  v(0)=3 : v(1)=4
  PRINT "|ap="; NEURAL.NORMAPPLY(nh, v, o); "="; FORMAT(o(0), "#.0"); ","; FORMAT(o(1), "#.0");
  PRINT "|dn="; NEURAL.NORMDENORM(nh, o, v); "="; FORMAT(v(0), "#.0"); ","; FORMAT(v(1), "#.0");
  v(0)=3 : v(1)=4
  PRINT "|zs="; NEURAL.NORMZSCORE(nh, v, o); "="; FORMAT(o(0), "#.0000"); ","; FORMAT(o(1), "#.0000");
  PRINT "|st="; NEURAL.NORMSTATS(nh, st); "=";
  PRINT FORMAT(st(0), "#.0"); ","; FORMAT(st(1), "#.0"); ","; FORMAT(st(2), "#.0"); ","; FORMAT(st(3), "#.0");
  PRINT ","; FORMAT(st(4), "#.0000"); ","; FORMAT(st(5), "#.0000");
  PRINT "|nf="; NEURAL.NORMFREE(nh);

  ' gather: righe 2 e 0 di [[1,2],[3,4],[5,6]]
  DIM d(5) AS DOUBLE, gi(1) AS LONG, gout(3) AS DOUBLE
  d(0)=1 : d(1)=2 : d(2)=3 : d(3)=4 : d(4)=5 : d(5)=6
  gi(0)=2 : gi(1)=0
  PRINT "|ga="; NEURAL.GATHERROWS(gout, d, 3, 2, gi, 2); "=";
  PRINT FORMAT(gout(0), "#.0"); ","; FORMAT(gout(1), "#.0"); ","; FORMAT(gout(2), "#.0"); ","; FORMAT(gout(3), "#.0");
  PRINT "|sl="; NEURAL.SLICEROWS(gout, d, 3, 2, 1, 2); "=";
  PRINT FORMAT(gout(0), "#.0"); ","; FORMAT(gout(1), "#.0"); ","; FORMAT(gout(2), "#.0"); ","; FORMAT(gout(3), "#.0");

  ' copyblock: 2×2 da (1,1) di [[1..9]] 3×3
  DIM b9(8) AS DOUBLE, bo(3) AS DOUBLE
  DIM i AS LONG
  FOR i = 0 TO 8
    b9(i) = i + 1
  NEXT i
  PRINT "|cb="; NEURAL.COPYBLOCK(bo, b9, 3, 3, 1, 1, 2, 2); "=";
  PRINT FORMAT(bo(0), "#.0"); ","; FORMAT(bo(1), "#.0"); ","; FORMAT(bo(2), "#.0"); ","; FORMAT(bo(3), "#.0");

  ' apply x*x+1 su [1,2,3]; absmax di [-5,3]
  DIM m3(2) AS DOUBLE, mo(2) AS DOUBLE, ab(1) AS DOUBLE
  m3(0)=1 : m3(1)=2 : m3(2)=3
  PRINT "|ap2="; NEURAL.MATAPPLY(mo, m3, 3, QuadPlus1); "=";
  PRINT FORMAT(mo(0), "#.0"); ","; FORMAT(mo(1), "#.0"); ","; FORMAT(mo(2), "#.0");
  ab(0)=-5 : ab(1)=3
  PRINT "|mx="; FORMAT(NEURAL.MATABSMAX(ab, 2), "#.0");
  PRINT "|f=0"
END SUB
