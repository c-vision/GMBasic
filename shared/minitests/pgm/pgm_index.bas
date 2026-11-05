' PGM (FFI) — indice piecewise-geometric su chiavi ordinate.
' EXPECT: h= 1 |rank= 4 |r5= 5 |r= 5 |lo= 4 |hi= 6 |free= 0 |bad=-1
' IMPORTA PGM (pacchetto gmbpgm); chiavi 0..9 con eps=1.
IMPORT PGM

SUB Main()
  DIM x(9) AS DOUBLE
  FOR i = 0 TO 9
    x(i) = i
  NEXT i

  DIM h AS LONG
  h = PGM.BUILD(x, 10, 1)
  PRINT "h="; h;

  PRINT "|rank="; PGM.RANK(h, 4.0);
  PRINT "|r5="; PGM.RANK(h, 5.0);

  DIM rng(1) AS LONG
  PRINT "|r="; PGM.RANGE(h, 5.0, rng);
  PRINT "|lo="; rng(0); "|hi="; rng(1);

  PRINT "|free="; PGM.FREE(h);
  PRINT "|bad="; PGM.RANK(h, 5.0)
END SUB
