' NEURAL (FFI) — pruning 2:4, quantizzazione affine int8/uint8, ternario bit-sliced.
' EXPECT: sp= 4 ; 0 , 3 , 4 ,0.3,0.4|l1= 0 : 6 , 15 |mask= 2 : 1 , 1 , 0 |qp= 0 :0.003922,-128 |q8= 0 :-128 , 0 , 127 |dq= 0 :0.0000,0.5020,1.0000|rq= 0 : 127 ,-128 |gemm= 0 : 10 |rsh= 0 :-2 , 0 , 25 , 75 |qconv= 0 : 1 , 2 , 3 , 4 |cl= 0 : 10 , 100 , 150 , 150 |pc= 0 :0.0078431, 128 , 0 |cc= 0 : 1 , 2 , 3 , 4 |t= 2001 : 8 |tm= 0 :0.50,0.00,-0.50|tf= 0 :0.00,-0.50|td=-1 |f= 0
IMPORT NEURAL

SUB Main()
  ' ── pruning ──
  DIM w(7) AS DOUBLE, l1(2) AS DOUBLE, mask(2) AS LONG
  w(0)=1 : w(1)=-2 : w(2)=3 : w(3)=4 : w(4)=0.1 : w(5)=0.2 : w(6)=0.3 : w(7)=0.4
  PRINT "sp="; NEURAL.SPARSIFY24(w, 8, 4, 2); ";";
  PRINT w(0); ","; w(2); ","; w(3); ","; FORMAT(w(6), "#.0"); ","; FORMAT(w(7), "#.0"); "|";

  DIM wc(5) AS DOUBLE
  wc(0)=1 : wc(1)=-2 : wc(2)=3 : wc(3)=4 : wc(4)=5 : wc(5)=-6
  PRINT "l1="; NEURAL.PRUNECHANNELL1(wc, 2, 3, l1); ":"; l1(0); ","; l1(1); "|";
  DIM l1b(2) AS DOUBLE
  l1b(0)=6 : l1b(1)=15 : l1b(2)=3
  PRINT "mask="; NEURAL.PRUNECHANNELMASK(l1b, 3, 0.67, mask); ":";
  PRINT mask(0); ","; mask(1); ","; mask(2); "|";

  ' ── quantizzazione affine ──
  DIM x(10) AS DOUBLE, qp(1) AS DOUBLE, q(10) AS LONG, back(10) AS DOUBLE
  DIM i AS LONG
  FOR i = 0 TO 10
    x(i) = i / 10.0
  NEXT i
  PRINT "qp="; NEURAL.QUANTPARAMS(x, 11, qp); ":";
  PRINT FORMAT(qp(0), "#.000000"); ","; qp(1); "|";

  PRINT "q8="; NEURAL.QUANTI8(x, 11, 1.0/255.0, -128, q); ":";
  PRINT q(0); ","; q(5); ","; q(10); "|";
  PRINT "dq="; NEURAL.DEQUANTI8(q, 11, 1.0/255.0, -128, back); ":";
  PRINT FORMAT(back(0), "#.0000"); ","; FORMAT(back(5), "#.0000"); ","; FORMAT(back(10), "#.0000"); "|";

  DIM acc(1) AS LONG, rq(1) AS LONG
  acc(0)=100 : acc(1)=-100
  PRINT "rq="; NEURAL.REQUANTI8(acc, 2, 0.1, 0, 0.05, 10, rq); ":";
  PRINT rq(0); ","; rq(1); "|";

  DIM a2(1) AS LONG, b2(1) AS LONG, bias0(0) AS LONG, g1(0) AS LONG
  a2(0)=10 : a2(1)=20 : b2(0)=1 : b2(1)=2 : bias0(0)=0
  PRINT "gemm="; NEURAL.QUANTGEMMU8(g1, a2, b2, bias0, 1, 2, 1, 0.1, 0, 0.5, 0, 0.25, 0); ":";
  PRINT g1(0); "|";

  DIM sh3(3) AS LONG, sho(3) AS LONG
  sh3(0)=-7 : sh3(1)=0 : sh3(2)=100 : sh3(3)=300
  PRINT "rsh="; NEURAL.QUANTREQSHIFTI8(sh3, 4, 2, sho); ":";
  PRINT sho(0); ","; sho(1); ","; sho(2); ","; sho(3); "|";

  DIM qin(3) AS LONG, qw(0) AS LONG, qbias(0) AS LONG, qout(3) AS LONG
  qin(0)=1 : qin(1)=2 : qin(2)=3 : qin(3)=4
  qw(0)=1 : qbias(0)=0
  PRINT "qconv="; NEURAL.QUANTCONV2DI8(qout, qin, qw, qbias, 1, 2, 2, 1, 1, 1, 1, 0, 0, 0); ":";
  PRINT qout(0); ","; qout(1); ","; qout(2); ","; qout(3); "|";

  DIM cl(3) AS LONG
  cl(0)=1 : cl(1)=100 : cl(2)=200 : cl(3)=255
  PRINT "cl="; NEURAL.QUANTCLAMPU8(cl, 4, 10, 150); ":";
  PRINT cl(0); ","; cl(1); ","; cl(2); ","; cl(3); "|";

  DIM pwc(3) AS DOUBLE, psc(1) AS DOUBLE, pzp(1) AS LONG
  pwc(0)=1 : pwc(1)=-1 : pwc(2)=0 : pwc(3)=2
  PRINT "pc="; NEURAL.QUANTPARAMSCHANNEL(pwc, 2, 2, psc, pzp); ":";
  PRINT FORMAT(psc(0), "#.0000000"); ","; pzp(0); ","; pzp(1); "|";

  DIM cin(3) AS LONG, cw(0) AS LONG, cb(0) AS LONG, cm(0) AS DOUBLE, cout(3) AS LONG
  cin(0)=10 : cin(1)=20 : cin(2)=30 : cin(3)=40
  cw(0)=1 : cb(0)=0 : cm(0)=0.1
  PRINT "cc="; NEURAL.QUANTCONV2DI8CHANNEL(cout, cin, cw, cb, cm, 1, 2, 2, 1, 1, 1, 1, 0, 0, 0); ":";
  PRINT cout(0); ","; cout(1); ","; cout(2); ","; cout(3); "|";

  ' ── ternario ──
  DIM tw(5) AS DOUBLE, twm(5) AS DOUBLE, tx(2) AS DOUBLE, tout(1) AS DOUBLE, tbits(0) AS LONG
  tw(0)=1 : tw(1)=0 : tw(2)=0 : tw(3)=0 : tw(4)=-2 : tw(5)=0
  th = NEURAL.TERNARYINIT(tw, 2, 3)
  PRINT "t="; th; ":"; NEURAL.TERNARYACTBYTES(10); "|";
  PRINT "tm="; NEURAL.TERNARYTOMAT(th, twm); ":";
  PRINT FORMAT(twm(0), "#.00"); ","; FORMAT(twm(1), "#.00"); ","; FORMAT(twm(4), "#.00"); "|";
  tx(0)=-1 : tx(1)=2 : tx(2)=3
  PRINT "tf="; NEURAL.TERNARYFORWARD(th, tx, 1, tout); ":";
  PRINT FORMAT(tout(0), "#.00"); ","; FORMAT(tout(1), "#.00"); "|";
  tbits(0)=6
  PRINT "td="; NEURAL.TERNARYDOTROW(th, 1, tbits, 3); "|";
  PRINT "f="; NEURAL.TERNARYFREE(th)
END SUB
