' NEURAL (FFI) — CNN: conv, pooling, im2col, resize, winograd, deconv, LRN + FusedMM CSR.
' EXPECT: conv= 0  , 3 , 5 , 7 , 9  |mp= 0  , 4 , 3  |ap= 0  , 2.5  |i2c= 0  , 1 , 2 , 3 , 4  |rs= 0  , 1 , 1 , 4  |w= 0  , 1 , 6 , 16  |d= 0  , 2 , 2 , 2 , 2  |l= 0 |0.9998,1.9997|fm= 0  , 4 , 9  |ag= 0  , 2 , 3  |f= 0
' IMPORTA NEURAL (pacchetto gmbneural): layout CHW, handle per la configurazione.
IMPORT NEURAL

SUB Main()
  DIM o4(3) AS DOUBLE, in4(3) AS DOUBLE, w1(0) AS DOUBLE, b1(0) AS DOUBLE
  in4(0)=1 : in4(1)=2 : in4(2)=3 : in4(3)=4
  w1(0)=2 : b1(0)=1
  DIM lh AS LONG
  lh = NEURAL.CONVINIT(1, 2, 2, 1, 1, 1, 1, 0)
  PRINT "conv="; NEURAL.CONVRUNB(lh, in4, w1, b1, o4); " ,"; o4(0); ","; o4(1); ","; o4(2); ","; o4(3); " |";
  NEURAL.FREE(lh)

  DIM po(0) AS DOUBLE, am(0) AS LONG
  lh = NEURAL.MAXPOOLINIT(1, 2, 2, 2, 2)
  PRINT "mp="; NEURAL.POOLRUNARG(lh, in4, po, am); " ,"; po(0); ","; am(0); " |";
  NEURAL.FREE(lh)
  lh = NEURAL.AVGPOOLINIT(1, 2, 2, 2, 2)
  PRINT "ap="; NEURAL.POOLRUN(lh, in4, po); " ,"; po(0); " |";
  NEURAL.FREE(lh)

  DIM col(3) AS DOUBLE, back(3) AS DOUBLE
  lh = NEURAL.IM2COLINIT(1, 2, 2, 1, 1, 1, 0)
  PRINT "i2c="; NEURAL.IM2COLRUN(lh, in4, col); " ,"; col(0); ","; col(1); ","; col(2); ","; col(3); " |";
  NEURAL.FREE(lh)

  DIM o16(15) AS DOUBLE
  lh = NEURAL.RESIZEINIT(1, 2, 2, 4, 4)
  PRINT "rs="; NEURAL.RESIZERUN(lh, 0, in4, o16); " ,"; o16(0); ","; o16(5); ","; o16(15); " |";
  NEURAL.FREE(lh)

  DIM i16(15) AS DOUBLE, w9(8) AS DOUBLE
  DIM xx AS LONG, yy AS LONG
  FOR yy = 0 TO 3
    FOR xx = 0 TO 3
      i16(yy*4+xx) = yy*4+xx+1
    NEXT xx
  NEXT yy
  FOR xx = 0 TO 8
    w9(xx) = 0
  NEXT xx
  w9(4) = 1
  lh = NEURAL.WINOINIT(1, 4, 4, 1, 2)
  PRINT "w="; NEURAL.WINORUN(lh, i16, w9, o16); " ,"; o16(0); ","; o16(5); ","; o16(15); " |";
  NEURAL.FREE(lh)

  DIM d1(0) AS DOUBLE, dw(3) AS DOUBLE, dout(3) AS DOUBLE
  d1(0) = 2
  dw(0)=1 : dw(1)=1 : dw(2)=1 : dw(3)=1
  lh = NEURAL.DECONVINIT(1, 1, 1, 1, 2, 2, 2, 0)
  PRINT "d="; NEURAL.DECONVRUN(lh, d1, dw, dout); " ,"; dout(0); ","; dout(1); ","; dout(2); ","; dout(3); " |";
  NEURAL.FREE(lh)

  DIM l2(1) AS DOUBLE, lo(1) AS DOUBLE
  l2(0)=1 : l2(1)=2
  lh = NEURAL.LRNINIT(2, 1, 1, 2, 0.0001, 0.75, 1.0)
  PRINT "l="; NEURAL.LRNRUN(lh, l2, lo); "|"; FORMAT(lo(0), "#.0000"); ","; FORMAT(lo(1), "#.0000"); "|";
  NEURAL.FREE(lh)

  DIM rp(2) AS LONG, ci(1) AS LONG, vv(1) AS DOUBLE, X(1) AS DOUBLE, Y(1) AS DOUBLE, Z(1) AS DOUBLE
  rp(0)=0 : rp(1)=1 : rp(2)=2 : ci(0)=0 : ci(1)=1 : vv(0)=1 : vv(1)=1
  X(0)=1 : X(1)=1 : Y(0)=2 : Y(1)=3
  lh = NEURAL.FUSEDMMINIT(2, 2, 1, 2, 1.0, 0.0)
  PRINT "fm="; NEURAL.FUSEDMMRUN(lh, rp, ci, vv, X, Y, Z); " ,"; Z(0); ","; Z(1); " |";
  NEURAL.FREE(lh)
  DIM Z2(1) AS DOUBLE
  lh = NEURAL.AGGINIT(2, 2, 1, 2, 1.0, 0.0)
  PRINT "ag="; NEURAL.AGGRUN(lh, rp, ci, vv, Y, Z2); " ,"; Z2(0); ","; Z2(1); " |";
  NEURAL.FREE(lh)

  PRINT "f="; NEURAL.FREE(9999)
END SUB
