' NEURAL (FFI) — CNN, pooling, im2col, resize, winograd, deconv, LRN e FusedMM.
' Chiamate DIRETTE: nessun handle di configurazione (le dimensioni sono parametri).
' EXPECT: conv= 0  , 3 , 5 , 7 , 9  |cr= 0  , 2 , 8  |mp= 0  , 4 , 3  |mp2= 0  , 4  |ap= 0  , 2.5  |osz= 3  |i2c= 0  , 1 , 2 , 3 , 4  |c2i= 0  , 4  |rs= 0  , 1 , 1 , 4  |w= 0  , 1 , 6 , 16  |wb= 0  , 2 , 7  |d= 0  , 2 , 2 , 2 , 2  |db= 0  , 3 , 3  |up= 0  , 0 , 4  |l= 0  |0.9998,1.9997 |dd= 0  , 20  |fm= 0  , 4 , 9  |ag= 0  , 2 , 3  |f=-1
' IMPORTA NEURAL (pacchetto gmbneural): layout CHW, array 1-D o 2-D di BASIC.
IMPORT NEURAL

SUB Main()
  ' ── conv 2×2 con kernel 1×1 (peso 2) e bias 1 → 3 5 7 9 ──
  DIM in4(3) AS DOUBLE, w1(0) AS DOUBLE, b1(0) AS DOUBLE, o4(3) AS DOUBLE
  in4(0)=1 : in4(1)=2 : in4(2)=3 : in4(3)=4
  w1(0)=2 : b1(0)=1
  PRINT "conv="; NEURAL.CONV2DB(o4, in4, w1, b1, 1, 2, 2, 1, 1, 1, 1, 0, 1); " ,"; o4(0); ","; o4(1); ","; o4(2); ","; o4(3); " |";
  ' senza bias (dilatazione 1)
  PRINT "cr="; NEURAL.CONV2D(o4, in4, w1, 1, 2, 2, 1, 1, 1, 1, 0, 1); " ,"; o4(0); ","; o4(3); " |";

  ' ── pooling ──
  DIM po(0) AS DOUBLE, am(0) AS LONG
  PRINT "mp="; NEURAL.MAXPOOLARG(po, in4, am, 1, 2, 2, 2, 2); " ,"; po(0); ","; am(0); " |";
  PRINT "mp2="; NEURAL.MAXPOOL(po, in4, 1, 2, 2, 2, 2); " ,"; po(0); " |";
  PRINT "ap="; NEURAL.AVGPOOL(po, in4, 1, 2, 2, 2, 2); " ,"; po(0); " |";
  PRINT "osz="; NEURAL.OUTSIZE(5, 3, 2, 1); " |";

  ' ── im2col / col2im ──
  DIM col(3) AS DOUBLE, back(3) AS DOUBLE
  PRINT "i2c="; NEURAL.IM2COL(col, in4, 1, 2, 2, 1, 1, 1, 0); " ,"; col(0); ","; col(1); ","; col(2); ","; col(3); " |";
  PRINT "c2i="; NEURAL.COL2IM(back, col, 1, 2, 2, 1, 1, 1, 0); " ,"; back(3); " |";

  ' ── resize nearest 2×2 → 4×4 ──
  DIM o16(15) AS DOUBLE
  PRINT "rs="; NEURAL.RESIZE(o16, in4, 1, 2, 2, 4, 4, 0); " ,"; o16(0); ","; o16(5); ","; o16(15); " |";

  ' ── winograd (4×4, kernel 3×3 con centro 1) senza e con bias ──
  DIM i16(15) AS DOUBLE, w9(8) AS DOUBLE, wb(0) AS DOUBLE
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
  PRINT "w="; NEURAL.WINOGRAD(o16, i16, w9, 1, 4, 4, 1, 2); " ,"; o16(0); ","; o16(5); ","; o16(15); " |";
  wb(0) = 1
  PRINT "wb="; NEURAL.WINOGRADB(o16, i16, w9, wb, 1, 4, 4, 1, 2); " ,"; o16(0); ","; o16(5); " |";

  ' ── deconv (1×1 → 2×2, stride 2) senza e con bias ──
  DIM d1(0) AS DOUBLE, dw(3) AS DOUBLE, dout(3) AS DOUBLE, dbi(0) AS DOUBLE
  d1(0) = 2
  dw(0)=1 : dw(1)=1 : dw(2)=1 : dw(3)=1
  PRINT "d="; NEURAL.DECONV2D(dout, d1, dw, 1, 1, 1, 1, 2, 2, 2, 0); " ,"; dout(0); ","; dout(1); ","; dout(2); ","; dout(3); " |";
  dbi(0) = 1
  PRINT "db="; NEURAL.DECONV2DB(dout, d1, dw, dbi, 1, 1, 1, 1, 2, 2, 2, 0); " ,"; dout(0); ","; dout(3); " |";

  ' ── unpool (fill 0, argmax 3) ──
  DIM upin(0) AS DOUBLE, upam(0) AS LONG, upout(3) AS DOUBLE
  upin(0)=4 : upam(0)=3
  PRINT "up="; NEURAL.UNPOOL(upout, upin, upam, 1, 2, 2, 2, 2, 0.0); " ,"; upout(0); ","; upout(3); " |";

  ' ── LRN ──
  DIM l2(1) AS DOUBLE, lo(1) AS DOUBLE
  l2(0)=1 : l2(1)=2
  PRINT "l="; NEURAL.LRN(lo, l2, 2, 1, 1, 2, 0.0001, 0.75, 1.0); " |"; FORMAT(lo(0), "#.0000"); ","; FORMAT(lo(1), "#.0000"); " |";

  ' ── convoluzione dilatata su 3×3 (kernel 2×2) ──
  DIM din(8) AS DOUBLE, dw2(3) AS DOUBLE, dout1(0) AS DOUBLE
  DIM k2 AS LONG
  FOR k2 = 0 TO 8
    din(k2) = k2 + 1
  NEXT k2
  dw2(0)=1 : dw2(1)=1 : dw2(2)=1 : dw2(3)=1
  PRINT "dd="; NEURAL.CONV2D(dout1, din, dw2, 1, 3, 3, 1, 2, 2, 1, 0, 2); " ,"; dout1(0); " |";

  ' ── FusedMM e aggregazione su CSR ──
  DIM rp(2) AS LONG, ci(1) AS LONG, vv(1) AS DOUBLE, X(1) AS DOUBLE, Y(1) AS DOUBLE, Z(1) AS DOUBLE
  rp(0)=0 : rp(1)=1 : rp(2)=2 : ci(0)=0 : ci(1)=1 : vv(0)=1 : vv(1)=1
  X(0)=1 : X(1)=1 : Y(0)=2 : Y(1)=3
  PRINT "fm="; NEURAL.FUSEDMM(Z, rp, ci, vv, X, Y, 2, 2, 1, 2, 1.0, 0.0); " ,"; Z(0); ","; Z(1); " |";
  DIM Z2(1) AS DOUBLE
  PRINT "ag="; NEURAL.AGGREGATE(Z2, rp, ci, vv, Y, 2, 2, 1, 2, 1.0, 0.0); " ,"; Z2(0); ","; Z2(1); " |";

  PRINT "f="; NEURAL.FREE(9999)
END SUB
