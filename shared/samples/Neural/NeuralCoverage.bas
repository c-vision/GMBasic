' FILE: Neural/NeuralCoverage.bas
' COPERTURA DIDATTICA — un programma solo, un SUB per famiglia, che chiama OGNI
' funzione del namespace NEURAL (213 export del pacchetto gmbneural via FFI).
'
' Serve come contratto di superficie: se una funzione viene rinominata o rimossa,
' questo sample non compila più. L'output è deterministico e si verifica sui 4 backend
' con tools/sh/parity4.
'
' Famiglie: ricorrenti (RNN/LSTM/GRU/BiRNN/PBP), CNN, pruning/quantizzazione/ternario,
' alberi/forest/adaboost, SNN/BNN/PINN/detection/KNN/metriche/memplan,
' attivazioni+denso+perdite, dropout/norm/chuck, autodiff+GP, norm/gather/block,
' matrici 2-D.
'
' Domini: NEURAL.
IMPORT NEURAL

FUNCTION GpFit(code() AS DOUBLE) AS DOUBLE
  DIM i AS LONG, s AS DOUBLE
  s = 0
  FOR i = 0 TO UBOUND(code)
    s = s + code(i)
  NEXT i
  RETURN s
END FUNCTION
FUNCTION QuadPlus1(x AS DOUBLE) AS DOUBLE
  RETURN x * x + 1
END FUNCTION
' NEURAL (FFI) — slice ricorrente: RNN, LSTM, GRU, BiRNN (BPTT + ottimizzatore) e PBP.
' EXPECT: rnn= 1 |f= 0 |y=0.2515,0.2417,0.4177|b= 0 |s= 0 |zg= 0 |f2= 0 |y2=0.1060|z= 0 |lstm= 0 |gru= 0 |birnn= 0 |pbp= 0 ,0.3989,0.3408|pl= 0 |mz=0.1000,0.2000|vz=0.0250,0.1000|fz=none
' IMPORTA NEURAL (pacchetto gmbneural): solo ciò che non esiste in GMBasic (TORCH copre MLP).

SUB UnaRicorrente()
  DIM x(2) AS DOUBLE, y(2) AS DOUBLE, gy(2) AS DOUBLE
  DIM h AS LONG, lh AS LONG, gh AS LONG, bh AS LONG, ph AS LONG
  x(0) = 0.1 : x(1) = 0.2 : x(2) = 0.3
  gy(0) = 1.0 : gy(1) = 0.0 : gy(2) = 0.0

  ' ── RNN: forward, backward, step SGD, forward ──
  h = NEURAL.RNNINIT(1, 2, 1, 42)
  PRINT "rnn="; h;
  PRINT "|f="; NEURAL.RNNFORWARD(h, x, 3, y);
  PRINT "|y="; FORMAT(y(0), "#.0000"); ","; FORMAT(y(1), "#.0000"); ","; FORMAT(y(2), "#.0000");
  PRINT "|b="; NEURAL.RNNBACKWARD(h, gy, 3);
  PRINT "|s="; NEURAL.STEP(h, 0, 0.1);
  PRINT "|zg="; NEURAL.ZEROGRAD(h);
  PRINT "|f2="; NEURAL.RNNFORWARD(h, x, 3, y);
  PRINT "|y2="; FORMAT(y(0), "#.0000");
  PRINT "|z="; NEURAL.FREE(h);

  ' ── LSTM / GRU / BiRNN: init, forward, backward, step, free ──
  lh = NEURAL.LSTMINIT(1, 2, 1, 7)
  PRINT "|lstm="; NEURAL.LSTMFORWARD(lh, x, 3, y) + NEURAL.LSTMBACKWARD(lh, gy, 3) + NEURAL.STEP(lh, 2, 0.01) + NEURAL.FREE(lh);
  gh = NEURAL.GRUINIT(1, 2, 1, 9)
  PRINT "|gru="; NEURAL.GRUFORWARD(gh, x, 3, y) + NEURAL.GRUBACKWARD(gh, gy, 3) + NEURAL.STEP(gh, 1, 0.01) + NEURAL.FREE(gh);
  bh = NEURAL.BIRNNINIT(2, 1, 2, 1, 11)
  PRINT "|birnn="; NEURAL.BIRNNFORWARD(bh, x, 3, y) + NEURAL.BIRNNBACKWARD(bh, gy, 3) + NEURAL.STEP(bh, 2, 0.01) + NEURAL.FREE(bh);

  ' ── PBP: momenti ReLU su N(0,1) ──
  DIM mo(1) AS DOUBLE
  PRINT "|pbp="; NEURAL.PBPMOMENTS(0.0, 1.0, mo); ","; FORMAT(mo(0), "#.0000"); ","; FORMAT(mo(1), "#.0000");

  ' ── PBP layer lineare 2x1 (relu=0): mz = W*mz_prev, vz = W^2*vz_prev ──
  DIM mw(1) AS DOUBLE, vw(1) AS DOUBLE, mzp(0) AS DOUBLE, vzp(0) AS DOUBLE
  DIM mzo(1) AS DOUBLE, vzo(1) AS DOUBLE
  mw(0) = 0.5 : mw(1) = 1.0
  vw(0) = 0.0 : vw(1) = 0.0
  mzp(0) = 0.2 : vzp(0) = 0.1
  ' senza handle: i pesi e i momenti sono parametri della chiamata
  PRINT "|pl="; NEURAL.PBPLAYER(mw, vw, mzp, vzp, mzo, vzo, 1, 2, 1.0, 0);
  PRINT "|mz="; FORMAT(mzo(0), "#.0000"); ","; FORMAT(mzo(1), "#.0000");
  PRINT "|vz="; FORMAT(vzo(0), "#.0000"); ","; FORMAT(vzo(1), "#.0000");
  PRINT "|fz=none"
END SUB

' NEURAL (FFI) — CNN, pooling, im2col, resize, winograd, deconv, LRN e FusedMM.
' Chiamate DIRETTE: nessun handle di configurazione (le dimensioni sono parametri).
' EXPECT: conv= 0  , 3 , 5 , 7 , 9  |cr= 0  , 2 , 8  |mp= 0  , 4 , 3  |mp2= 0  , 4  |ap= 0  , 2.5  |osz= 3  |i2c= 0  , 1 , 2 , 3 , 4  |c2i= 0  , 4  |rs= 0  , 1 , 1 , 4  |w= 0  , 1 , 6 , 16  |wb= 0  , 2 , 7  |d= 0  , 2 , 2 , 2 , 2  |db= 0  , 3 , 3  |up= 0  , 0 , 4  |l= 0  |0.9998,1.9997 |dd= 0  , 20  |fm= 0  , 4 , 9  |ag= 0  , 2 , 3  |f=-1
' IMPORTA NEURAL (pacchetto gmbneural): layout CHW, array 1-D o 2-D di BASIC.

SUB UnaCnn()
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

' NEURAL (FFI) — pruning 2:4, quantizzazione affine int8/uint8, ternario bit-sliced.
' EXPECT: sp= 4 ; 0 , 3 , 4 ,0.3,0.4|l1= 0 : 6 , 15 |mask= 2 : 1 , 1 , 0 |qp= 0 :0.003922,-128 |q8= 0 :-128 , 0 , 127 |dq= 0 :0.0000,0.5020,1.0000|rq= 0 : 127 ,-128 |gemm= 0 : 10 |rsh= 0 :-2 , 0 , 25 , 75 |qconv= 0 : 1 , 2 , 3 , 4 |cl= 0 : 10 , 100 , 150 , 150 |pc= 0 :0.0078431, 128 , 0 |cc= 0 : 1 , 2 , 3 , 4 |t= 2001 : 8 |tm= 0 :0.50,0.00,-0.50|tf= 0 :0.00,-0.50|td=-1 |f= 0

SUB UnPruneQuant()
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

' NEURAL (FFI) — CART (regressione/classificazione), Random Forest, AUC, AdaBoost.
' EXPECT: tr= 3001 : 0 ,10.0,20.0|tc= 3001 : 0 , 0 , 1 |fr= 3101 : 0 ,10.0,20.0|fc= 3101 : 0 , 0 , 1 |auc=0.75|ab= 3201 : 0 ,-1 , 1 |f= 0

SUB UnAlbero()
  DIM X(5) AS DOUBLE, yr(5) AS DOUBLE, yc(5) AS LONG, xq(0) AS DOUBLE
  DIM i AS LONG
  FOR i = 0 TO 5
    X(i) = i + 1
  NEXT i
  yr(0)=10 : yr(1)=10 : yr(2)=10 : yr(3)=20 : yr(4)=20 : yr(5)=20
  yc(0)=0 : yc(1)=0 : yc(2)=0 : yc(3)=1 : yc(4)=1 : yc(5)=1

  h = NEURAL.TREEINIT(0, 0, 0)
  PRINT "tr="; h; ":"; NEURAL.TREEFITREG(h, X, yr, 6, 1);
  xq(0)=2 : PRINT ","; FORMAT(NEURAL.TREEPREDICT(h, xq), "#.0");
  xq(0)=5 : PRINT ","; FORMAT(NEURAL.TREEPREDICT(h, xq), "#.0"); "|";
  NEURAL.TREEFREE(h)

  h = NEURAL.TREEINIT(3, 2, 1)
  PRINT "tc="; h; ":"; NEURAL.TREEFITCLS(h, X, yc, 6, 1, 2);
  xq(0)=2 : PRINT ","; NEURAL.TREEPREDICT(h, xq);
  xq(0)=5 : PRINT ","; NEURAL.TREEPREDICT(h, xq); "|";
  NEURAL.TREEFREE(h)

  h = NEURAL.FORESTINIT(5, 3, 2, 0, 1)
  PRINT "fr="; h; ":"; NEURAL.FORESTFITREG(h, X, yr, 6, 1);
  xq(0)=1 : PRINT ","; FORMAT(NEURAL.FORESTPREDICT(h, xq), "#.0");
  xq(0)=6 : PRINT ","; FORMAT(NEURAL.FORESTPREDICT(h, xq), "#.0"); "|";
  NEURAL.FORESTFREE(h)

  h = NEURAL.FORESTINIT(5, 3, 2, 0, 1)
  PRINT "fc="; h; ":"; NEURAL.FORESTFITCLS(h, X, yc, 6, 1, 2);
  xq(0)=1 : PRINT ","; NEURAL.FORESTPREDICT(h, xq);
  xq(0)=6 : PRINT ","; NEURAL.FORESTPREDICT(h, xq); "|";
  NEURAL.FORESTFREE(h)

  DIM act(3) AS DOUBLE, sco(3) AS DOUBLE
  act(0)=0 : act(1)=0 : act(2)=1 : act(3)=1
  sco(0)=0.1 : sco(1)=0.4 : sco(2)=0.35 : sco(3)=0.8
  PRINT "auc="; FORMAT(NEURAL.AUC(act, sco, 4), "#.00"); "|";

  h = NEURAL.ADABOOSTINIT(10, 1.0)
  PRINT "ab="; h; ":"; NEURAL.ADABOOSTFIT(h, X, yc, 6, 1);
  xq(0)=2 : PRINT ","; NEURAL.ADABOOSTPREDICT(h, xq);
  xq(0)=5 : PRINT ","; NEURAL.ADABOOSTPREDICT(h, xq); "|";
  PRINT "f="; NEURAL.ADABOOSTFREE(h)
END SUB

' NEURAL (FFI) — SNN, BNN, PINN, detection (IoU/NMS/YOLO), KNN, metriche, memplan.
' EXPECT: snn=0.50,0.250,-0.125|lut= 2 |bnn= 2 , 1 : 0 = 5 , 3 , 3 ,-2.0,-32767.0|pinn=0.0,1.0,1.0,9.0,56.25,4.0,9.0|pn=3.0|iou=1.0|nms= 2 : 0 , 2 |yolo= 0 =0.1154,0.1154,0.1538,0.2308||mc= 0 :0.9,0.0,0.7|knn= 0 , 1 ,1.00|kf=none|cm= 0 : 1 , 1 , 1 , 1 , 2 |acc=0.6667,0.6667|pr=0.7222:1.00,0.50,0.67|rc=0.6667:0.50,1.00|f1=0.6556:0.50,0.80|reg=1.3333,0.6667,-1.0|mem=30.0:20.0,0.0,0.0|f=0

SUB UnaUtility()
  PRINT "snn="; FORMAT(NEURAL.SNNSURROGATE(0), "#.00"); ","; FORMAT(NEURAL.SNNSURROGATE(1), "#.000"); ","; FORMAT(NEURAL.SNNSURROGATEGRAD(1), "#.000"); "|";
  DIM sx(2) AS DOUBLE, sa(1) AS LONG, sb(1) AS LONG
  sx(0)=1 : sx(1)=2 : sx(2)=0
  sa(0)=0 : sa(1)=1 : sb(0)=1 : sb(1)=2
  PRINT "lut="; NEURAL.SNNLUTINDEX(sx, sa, sb, 2); "|";

  DIM bx(2) AS DOUBLE, bw(0) AS LONG, bw2(0) AS LONG
  bx(0)=-1 : bx(1)=2 : bx(2)=-3
  PRINT "bnn="; NEURAL.BNNBYTES(10); ","; NEURAL.BNNWORDS(10); ":";
  bw(0)=0
  PRINT NEURAL.BNNPACK(bw, bx, 3); "="; bw(0);
  bw2(0)=bw(0)
  PRINT ","; NEURAL.BNNXNOR(bw, bw2, 3); ","; NEURAL.BNNDOT(bw, bw2, 3);
  PRINT ","; FORMAT(NEURAL.BNNTHRESH(2, 4), "#.0"); ","; FORMAT(NEURAL.BNNTHRESH(0, 5), "#.0"); "|";

  PRINT "pinn="; FORMAT(NEURAL.PINNHEAT(1, 1, 1), "#.0"); ","; FORMAT(NEURAL.PINNWAVE(2, 1, 1), "#.0");
  PRINT ","; FORMAT(NEURAL.PINNLAPLACE(1, 1, 3), "#.0"); ","; FORMAT(NEURAL.PINNADVECTION(1, 2, 1), "#.0");
  PRINT ","; FORMAT(NEURAL.PINNBURGERS(1, 2, 3, 4, 0.5), "#.00"); ","; FORMAT(NEURAL.PINNBOUNDARY(3, 1), "#.0");
  PRINT ","; FORMAT(NEURAL.PINNINITIAL(2, 5), "#.0"); "|";
  DIM ls(2) AS DOUBLE
  ls(0)=1 : ls(1)=-3 : ls(2)=2
  PRINT "pn="; FORMAT(NEURAL.PINNADAPTIVENORM(ls, 3), "#.0"); "|";

  DIM b1(3) AS DOUBLE, b2(3) AS DOUBLE
  b1(0)=0 : b1(1)=0 : b1(2)=2 : b1(3)=2
  b2(0)=0 : b2(1)=0 : b2(2)=2 : b2(3)=2
  PRINT "iou="; FORMAT(NEURAL.IOU(b1, b2), "#.0"); "|";

  DIM boxes(11) AS DOUBLE, scores(2) AS DOUBLE, keep(2) AS LONG
  DIM k AS LONG
  boxes(0)=0 : boxes(1)=0 : boxes(2)=2 : boxes(3)=2
  boxes(4)=0.1 : boxes(5)=0 : boxes(6)=2 : boxes(7)=2
  boxes(8)=10 : boxes(9)=10 : boxes(10)=2 : boxes(11)=2
  scores(0)=0.9 : scores(1)=0.8 : scores(2)=0.7
  k = NEURAL.NMS(boxes, scores, 3, 0.5, keep)
  PRINT "nms="; k; ":"; keep(0); ","; keep(1); "|";

  DIM yo(3) AS DOUBLE
  PRINT "yolo="; NEURAL.YOLOBOX(yo, 0, 0, 0, 0, 2, 3, 1, 1, 13, 13); "=";
  PRINT FORMAT(yo(0), "#.0000"); ","; FORMAT(yo(1), "#.0000"); ","; FORMAT(yo(2), "#.0000"); ","; FORMAT(yo(3), "#.0000"); "|";

  DIM pr(2) AS DOUBLE
  pr(0)=0.9 : pr(1)=0.8 : pr(2)=0.7
  PRINT "|mc="; NEURAL.NMSMULTICLASS(boxes, pr, 3, 1, 0.5); ":"; FORMAT(pr(0), "#.0"); ","; FORMAT(pr(1), "#.0"); ","; FORMAT(pr(2), "#.0");

  DIM Xtr(5) AS DOUBLE, ytr(5) AS LONG, Xte(1) AS DOUBLE, yte(1) AS LONG, xq(0) AS DOUBLE
  DIM i AS LONG
  FOR i = 0 TO 5
    IF i < 3 THEN
      Xtr(i) = i
      ytr(i) = 0
    ELSE
      Xtr(i) = i + 7
      ytr(i) = 1
    END IF
  NEXT i
  xq(0)=2.1
  PRINT "|knn="; NEURAL.KNNCLASSIFY(Xtr, ytr, 6, 1, 2, 1, xq);
  xq(0)=10.2
  PRINT ","; NEURAL.KNNCLASSIFY(Xtr, ytr, 6, 1, 2, 1, xq);
  Xte(0)=0.1 : Xte(1)=10.9 : yte(0)=0 : yte(1)=1
  PRINT ","; FORMAT(NEURAL.KNNACCURACY(Xtr, ytr, 6, Xte, yte, 2, 1, 2, 1), "#.00");
  PRINT "|kf=none";

  DIM yt(5) AS LONG, yp(5) AS LONG, cm(8) AS LONG, per(2) AS DOUBLE
  yt(0)=0 : yt(1)=0 : yt(2)=1 : yt(3)=1 : yt(4)=2 : yt(5)=2
  yp(0)=0 : yp(1)=1 : yp(2)=1 : yp(3)=2 : yp(4)=2 : yp(5)=2
  PRINT "|cm="; NEURAL.CONFUSION(yt, yp, 6, 3, cm); ":";
  PRINT cm(0); ","; cm(1); ","; cm(4); ","; cm(5); ","; cm(8); "|";
  PRINT "acc="; FORMAT(NEURAL.ACCURACY(yt, yp, 6), "#.0000"); ","; FORMAT(NEURAL.ACCURACYCM(cm, 3), "#.0000"); "|";
  PRINT "pr="; FORMAT(NEURAL.PRECISION(cm, 3, per), "#.0000"); ":"; FORMAT(per(0), "#.00"); ","; FORMAT(per(1), "#.00"); ","; FORMAT(per(2), "#.00"); "|";
  PRINT "rc="; FORMAT(NEURAL.RECALL(cm, 3, per), "#.0000"); ":"; FORMAT(per(0), "#.00"); ","; FORMAT(per(2), "#.00"); "|";
  PRINT "f1="; FORMAT(NEURAL.F1(cm, 3, per), "#.0000"); ":"; FORMAT(per(1), "#.00"); ","; FORMAT(per(2), "#.00"); "|";

  DIM vt(2) AS DOUBLE, vp(2) AS DOUBLE
  vt(0)=1 : vt(1)=2 : vt(2)=3
  vp(0)=1 : vp(1)=2 : vp(2)=5
  PRINT "reg="; FORMAT(NEURAL.MSE(vt, vp, 3), "#.0000"); ","; FORMAT(NEURAL.MAE(vt, vp, 3), "#.0000");
  PRINT ","; FORMAT(NEURAL.R2(vt, vp, 3), "#.0"); "|";

  ' array di UDT: TYPE MemTensor { Size AS DOUBLE : Birth AS LONG : Death AS LONG }
  DIM mt(2) AS MemTensor, mo(2) AS DOUBLE
  mt(0).Size=10 : mt(0).Birth=0 : mt(0).Death=1
  mt(1).Size=20 : mt(1).Birth=0 : mt(1).Death=1
  mt(2).Size=5  : mt(2).Birth=2 : mt(2).Death=3
  PRINT "mem="; FORMAT(NEURAL.MEMPLAN(mt, 3, mo), "#.0"); ":";
  PRINT FORMAT(mo(0), "#.0"); ","; FORMAT(mo(1), "#.0"); ","; FORMAT(mo(2), "#.0"); "|";
  PRINT "f=0"
END SUB

' NEURAL (FFI) — attivazioni/gradienti (gn_act), strato denso addestrabile, perdite.
' EXPECT: g=0.25,1.0,0.0|silu=0.7311,0.9277|elu=-0.6321,0.3679,-0.02,0.50|crelu= 0 :0.0,2.0,1.0,0.0|d= 5001 : 0 , 0 =1.0,2.0,4.0,3.0,4.0,8.0|b= 0 =1.0,0.0,0.0,1.0|s= 0 : 0 =0.90,-0.20,-0.30,0.60,1.00,1.00:-0.10,-0.10,1.00|zf= 0 |df= 0 |mse= 0 :0.0400,0.20,-0.20|mae= 0 :0.2000,0.50,-0.50|ce= 0 :0.2231,0.20,-0.20|bce= 0 :0.2231,0.6250,-0.6250||cg= 0 =1.0000,-2.0000|x1=1.0000,1.0000,1.0833,0.1667,0.6667,0.8333,1.0000,0.0000,3.0000,1.0000,0.0100,1.0010,0.0010,0.4621,0.3932,0.8651,1.0490,0.6250,0.1250,-0.1000,0.1000,1.1000,1.1000,1.0000,1.0000,1.0507,1.0507,0.0000,0.0000,1.0000,0.0000|f=0

SUB UnaAttivazione()
  PRINT "g="; FORMAT(NEURAL.SIGMOIDGRAD(0.5), "#.00"); ","; FORMAT(NEURAL.TANHGRAD(0), "#.0");
  PRINT ","; FORMAT(NEURAL.RELUGRAD(-1), "#.0"); "|";
  PRINT "silu="; FORMAT(NEURAL.SILU(1), "#.0000"); ","; FORMAT(NEURAL.SILUGRAD(1), "#.0000"); "|";
  PRINT "elu="; FORMAT(NEURAL.ELU(-1), "#.0000"); ","; FORMAT(NEURAL.ELUGRAD(-0.6321206), "#.0000");
  PRINT ","; FORMAT(NEURAL.LEAKYRELU(-2), "#.00"); ","; FORMAT(NEURAL.HARDSIGMOID(0), "#.00"); "|";

  DIM ci(1) AS DOUBLE, co(3) AS DOUBLE
  ci(0)=-1 : ci(1)=2
  PRINT "crelu="; NEURAL.CRELU(co, ci, 2); ":"; FORMAT(co(0), "#.0"); ","; FORMAT(co(1), "#.0"); ","; FORMAT(co(2), "#.0"); ","; FORMAT(co(3), "#.0"); "|";

  ' ── denso: W noti, forward, backward, SGD ──
  DIM W(5) AS DOUBLE, b(2) AS DOUBLE, x(3) AS DOUBLE, y(5) AS DOUBLE, gy(5) AS DOUBLE, gx(3) AS DOUBLE
  W(0)=1 : W(1)=0 : W(2)=0 : W(3)=1 : W(4)=1 : W(5)=1
  b(0)=0 : b(1)=0 : b(2)=1
  x(0)=1 : x(1)=2 : x(2)=3 : x(3)=4
  gy(0)=1 : gy(1)=0 : gy(2)=0 : gy(3)=0 : gy(4)=1 : gy(5)=0
  h = NEURAL.DENSEINIT(2, 3, 7)
  PRINT "d="; h; ":"; NEURAL.DENSESETPARAMS(h, W, b);
  PRINT ","; NEURAL.DENSEFORWARD(h, x, 2, y); "=";
  PRINT FORMAT(y(0), "#.0"); ","; FORMAT(y(1), "#.0"); ","; FORMAT(y(2), "#.0");
  PRINT ","; FORMAT(y(3), "#.0"); ","; FORMAT(y(4), "#.0"); ","; FORMAT(y(5), "#.0"); "|";
  PRINT "b="; NEURAL.DENSEBACKWARD(h, gy, 2, gx); "=";
  PRINT FORMAT(gx(0), "#.0"); ","; FORMAT(gx(1), "#.0"); ","; FORMAT(gx(2), "#.0"); ","; FORMAT(gx(3), "#.0"); "|";
  PRINT "s="; NEURAL.DENSESTEP(h, 0, 0.1); ":";
  PRINT NEURAL.DENSEPARAMS(h, W, b); "=";
  PRINT FORMAT(W(0), "#.00"); ","; FORMAT(W(1), "#.00"); ","; FORMAT(W(2), "#.00");
  PRINT ","; FORMAT(W(3), "#.00"); ","; FORMAT(W(4), "#.00"); ","; FORMAT(W(5), "#.00");
  PRINT ":"; FORMAT(b(0), "#.00"); ","; FORMAT(b(1), "#.00"); ","; FORMAT(b(2), "#.00"); "|";
  PRINT "zf="; NEURAL.DENSEZEROGRAD(h); "|df="; NEURAL.DENSEFREE(h); "|";

  ' ── perdite ──
  DIM ly(1) AS DOUBLE, lt(1) AS DOUBLE, lg(1) AS DOUBLE, lv(0) AS DOUBLE
  ly(0)=0.2 : ly(1)=0.8 : lt(0)=0 : lt(1)=1
  PRINT "mse="; NEURAL.LOSSMSE(ly, lt, 1, 2, lg, lv); ":"; FORMAT(lv(0), "#.0000");
  PRINT ","; FORMAT(lg(0), "#.00"); ","; FORMAT(lg(1), "#.00"); "|";
  PRINT "mae="; NEURAL.LOSSMAE(ly, lt, 1, 2, lg, lv); ":"; FORMAT(lv(0), "#.0000");
  PRINT ","; FORMAT(lg(0), "#.00"); ","; FORMAT(lg(1), "#.00"); "|";
  PRINT "ce="; NEURAL.LOSSCROSSENTROPY(ly, lt, 1, 2, lg, lv); ":"; FORMAT(lv(0), "#.0000");
  PRINT ","; FORMAT(lg(0), "#.00"); ","; FORMAT(lg(1), "#.00"); "|";
  PRINT "bce="; NEURAL.LOSSBINARYCROSSENTROPY(ly, lt, 1, 2, lg, lv); ":"; FORMAT(lv(0), "#.0000");
  PRINT ","; FORMAT(lg(0), "#.0000"); ","; FORMAT(lg(1), "#.0000"); "|";
  ' ── attivazioni/gradienti non ancora toccati (copertura di superficie) ──
  DIM cgi(1) AS DOUBLE, cgo(3) AS DOUBLE, cgin(1) AS DOUBLE
  cgi(0)=2 : cgi(1)=-1 : cgo(0)=1 : cgo(1)=3 : cgo(2)=0.5 : cgo(3)=2
  cgin(0)=2 : cgin(1)=-1
  PRINT "|cg="; NEURAL.CRELUGRAD(cgin, cgo, cgi, 2); "="; FORMAT(cgin(0), "#.0000"); ","; FORMAT(cgin(1), "#.0000");
  PRINT "|x1="; FORMAT(NEURAL.CELU(1, 1), "#.0000"); ","; FORMAT(NEURAL.CELUGRAD(1, 1), "#.0000");
  PRINT ","; FORMAT(NEURAL.GELUGRAD(1), "#.0000"); ","; FORMAT(NEURAL.HARDSIGMOIDGRAD(0), "#.0000");
  PRINT ","; FORMAT(NEURAL.HARDSWISH(1), "#.0000"); ","; FORMAT(NEURAL.HARDSWISHGRAD(1), "#.0000");
  PRINT ","; FORMAT(NEURAL.HARDTAN(2), "#.0000"); ","; FORMAT(NEURAL.HARDTANGRAD(2), "#.0000");
  PRINT ","; FORMAT(NEURAL.IDENTITY(3), "#.0000"); ","; FORMAT(NEURAL.IDENTITYGRAD(3), "#.0000");
  PRINT ","; FORMAT(NEURAL.LEAKYRELUGRAD(-1), "#.0000"); ","; FORMAT(NEURAL.LHTAN(2), "#.0000");
  PRINT ","; FORMAT(NEURAL.LHTANGRAD(2), "#.0000"); ","; FORMAT(NEURAL.LOGGY(1), "#.0000");
  PRINT ","; FORMAT(NEURAL.LOGGYGRAD(1), "#.0000"); ","; FORMAT(NEURAL.MISH(1), "#.0000");
  PRINT ","; FORMAT(NEURAL.MISHGRAD(1), "#.0000"); ","; FORMAT(NEURAL.PLSE(1), "#.0000");
  PRINT ","; FORMAT(NEURAL.PLSEGRAD(1), "#.0000"); ","; FORMAT(NEURAL.PRELU(-1, 0.1), "#.0000");
  PRINT ","; FORMAT(NEURAL.PRELUGRAD(-1, 0.1), "#.0000"); ","; FORMAT(NEURAL.RAMP(1), "#.0000");
  PRINT ","; FORMAT(NEURAL.RAMPGRAD(1), "#.0000"); ","; FORMAT(NEURAL.RELIE(1), "#.0000");
  PRINT ","; FORMAT(NEURAL.RELIEGRAD(1), "#.0000"); ","; FORMAT(NEURAL.SELU(1), "#.0000");
  PRINT ","; FORMAT(NEURAL.SELUGRAD(1), "#.0000"); ","; FORMAT(NEURAL.STAIR(1), "#.0000");
  PRINT ","; FORMAT(NEURAL.STAIRGRAD(1), "#.0000"); ","; FORMAT(NEURAL.THRESHOLD(0.5), "#.0000");
  PRINT ","; FORMAT(NEURAL.THRESHOLDGRAD(0.5), "#.0000"); "|";
  PRINT "f=0"
END SUB

' NEURAL (FFI) — dropout, BatchNorm, LayerNorm, ottimizzatore Chuck.
' EXPECT: off= 6001 : 0 =1.0,2.0,3.0,4.0|df= 0 |on= 6001 : 0 =0.00,4.00,6.00,0.00|db= 0 =0.00,2.00,2.00,0.00|df2= 0 |bn= 6101 : 0 =-1.0000,-1.0000,1.0000,1.0000|bp= 0 =1.0,0.0|bs= 0 : 0 =2.0,1.0|bb= 0 =0.0000,0.0000,0.0000,0.0000|bz= 0 |bf= 0 |ln= 6201 : 0 =-1.0000,1.0000|lp= 0 =1.0,0.0|ls= 0 : 0 =3.0,2.0|lb= 0 =-0.0000,0.0000|lz= 0 |lf= 0 |ck= 6301 : 0 |st= 0 =0.9253|cs= 0 =1.00,0.25,0.001,2.0,0.0|cr= 0 |cf= 0 |f=0

SUB UnaNormDrop()
  DIM dx(3) AS DOUBLE, do1(3) AS DOUBLE, dg(3) AS DOUBLE
  dx(0)=1 : dx(1)=2 : dx(2)=3 : dx(3)=4
  h = NEURAL.DROPOUTINIT(4, 0.0)
  PRINT "off="; h; ":"; NEURAL.DROPOUTFORWARD(h, dx, do1, 0, 1); "=";
  PRINT FORMAT(do1(0), "#.0"); ","; FORMAT(do1(1), "#.0"); ","; FORMAT(do1(2), "#.0"); ","; FORMAT(do1(3), "#.0");
  PRINT "|df="; NEURAL.DROPOUTFREE(h);
  h = NEURAL.DROPOUTINIT(4, 0.5)
  PRINT "|on="; h; ":"; NEURAL.DROPOUTFORWARD(h, dx, do1, 1, 7); "=";
  PRINT FORMAT(do1(0), "#.00"); ","; FORMAT(do1(1), "#.00"); ","; FORMAT(do1(2), "#.00"); ","; FORMAT(do1(3), "#.00");
  dg(0)=1 : dg(1)=1 : dg(2)=1 : dg(3)=1
  PRINT "|db="; NEURAL.DROPOUTBACKWARD(h, dg, do1); "=";
  PRINT FORMAT(do1(0), "#.00"); ","; FORMAT(do1(1), "#.00"); ","; FORMAT(do1(2), "#.00"); ","; FORMAT(do1(3), "#.00");
  PRINT "|df2="; NEURAL.DROPOUTFREE(h);

  ' ── BatchNorm: x 2×2, media [2,3], var [1,1] ──
  DIM bx(3) AS DOUBLE, by(3) AS DOUBLE, bgx(3) AS DOUBLE, gam(1) AS DOUBLE, bet(1) AS DOUBLE
  bx(0)=1 : bx(1)=2 : bx(2)=3 : bx(3)=4
  bh = NEURAL.BNINIT(2, 0.00001, 0.1)
  PRINT "|bn="; bh; ":"; NEURAL.BNFORWARD(bh, bx, 2, by, 1); "=";
  PRINT FORMAT(by(0), "#.0000"); ","; FORMAT(by(1), "#.0000"); ","; FORMAT(by(2), "#.0000"); ","; FORMAT(by(3), "#.0000");
  PRINT "|bp="; NEURAL.BNPARAMS(bh, gam, bet); "="; FORMAT(gam(0), "#.0"); ","; FORMAT(bet(0), "#.0");
  gam(0)=2 : bet(0)=1
  PRINT "|bs="; NEURAL.BNSETPARAMS(bh, gam, bet); ":"; NEURAL.BNPARAMS(bh, gam, bet); "="; FORMAT(gam(0), "#.0"); ","; FORMAT(bet(0), "#.0");
  by(0)=1 : by(1)=1 : by(2)=1 : by(3)=1
  PRINT "|bb="; NEURAL.BNBACKWARD(bh, by, 2, bgx); "=";
  PRINT FORMAT(bgx(0), "#.0000"); ","; FORMAT(bgx(1), "#.0000"); ","; FORMAT(bgx(2), "#.0000"); ","; FORMAT(bgx(3), "#.0000");
  PRINT "|bz="; NEURAL.BNZEROGRAD(bh); "|bf="; NEURAL.BNFREE(bh);

  ' ── LayerNorm: una riga [1,3] → media 2, var 1 ──
  DIM lx(1) AS DOUBLE, ly(1) AS DOUBLE, lgx(1) AS DOUBLE
  lx(0)=1 : lx(1)=3
  lh = NEURAL.LNINIT(2, 0.00001)
  PRINT "|ln="; lh; ":"; NEURAL.LNFORWARD(lh, lx, 1, ly); "=";
  PRINT FORMAT(ly(0), "#.0000"); ","; FORMAT(ly(1), "#.0000");
  PRINT "|lp="; NEURAL.LNPARAMS(lh, gam, bet); "="; FORMAT(gam(0), "#.0"); ","; FORMAT(bet(1), "#.0");
  gam(1)=3 : bet(1)=2
  PRINT "|ls="; NEURAL.LNSETPARAMS(lh, gam, bet); ":"; NEURAL.LNPARAMS(lh, gam, bet); "="; FORMAT(gam(1), "#.0"); ","; FORMAT(bet(1), "#.0");
  ly(0)=1 : ly(1)=1
  PRINT "|lb="; NEURAL.LNBACKWARD(lh, ly, 1, lgx); "=";
  PRINT FORMAT(lgx(0), "#.0000"); ","; FORMAT(lgx(1), "#.0000");
  PRINT "|lz="; NEURAL.LNZEROGRAD(lh);
  PRINT "|lf="; NEURAL.LNFREE(lh);

  ' ── Chuck: 1×1, un passo Adam ──
  DIM cp(0) AS DOUBLE, cg(0) AS DOUBLE, cs(5) AS DOUBLE
  cp(0)=1 : cg(0)=0.5
  ch = NEURAL.CHUCKINIT()
  PRINT "|ck="; ch; ":"; NEURAL.CHUCKLOSS(ch, 0.25);
  PRINT "|st="; NEURAL.CHUCKSTEP(ch, cp, cg, 1, 1, 0.1, 0.9, 0.999, 0.00000001); "=";
  PRINT FORMAT(cp(0), "#.0000");
  PRINT "|cs="; NEURAL.CHUCKSTATE(ch, cs); "=";
  PRINT FORMAT(cs(0), "#.00"); ","; FORMAT(cs(1), "#.00"); ","; FORMAT(cs(2), "#.000"); ","; FORMAT(cs(4), "#.0"); ","; FORMAT(cs(5), "#.0");
  PRINT "|cr="; NEURAL.CHUCKREST(ch); "|cf="; NEURAL.CHUCKFREE(ch);
  PRINT "|f=0"
END SUB

' NEURAL (FFI) — matrici 2-D di BASIC passate direttamente alle funzioni del pacchetto.
' L'ABI C riceve il buffer contiguo row-major (zero-copy): le dimensioni restano
' parametri espliciti perché l'FFI non le passa.
' EXPECT: fw= 0 = 3 , 7 |loss= 0 =1.0000,-1 ,-1 |bw= 0 :-1 ,-1 ,-1 ,-1 |st= 0 : 0 =1.4,1.6:0.2|y2=4.80,10.80|f= 0

SUB UnaMatrice()
  ' x: 2 righe × 2 colonne = [1,2 ; 3,4]
  DIM x(1, 1) AS DOUBLE
  x(0,0)=1 : x(0,1)=2
  x(1,0)=3 : x(1,1)=4
  ' y/t/grad: 2 righe × 1 colonna (dichiarate 4 righe per riuso)
  DIM t(3, 0) AS DOUBLE
  DIM y(3, 0) AS DOUBLE
  DIM g(3, 0) AS DOUBLE
  DIM gx(1, 1) AS DOUBLE
  DIM loss(0) AS DOUBLE, W(1) AS DOUBLE, b(0) AS DOUBLE
  W(0)=1 : W(1)=1 : b(0)=0

  h = NEURAL.DENSEINIT(2, 1, 7)
  NEURAL.DENSESETPARAMS(h, W, b)
  PRINT "fw="; NEURAL.DENSEFORWARD(h, x, 2, y); "="; y(0,0); ","; y(1,0);

  ' perdita su matrici: bersaglio [4,8] → errori -1,-1 → MSE 1.0
  t(0,0)=4 : t(1,0)=8
  PRINT "|loss="; NEURAL.LOSSMSE(y, t, 2, 1, g, loss); "="; FORMAT(loss(0), "#.0000");
  PRINT ","; g(0,0); ","; g(1,0);

  ' backward + SGD: gradW = gyᵀx = [-4,-6], gradb = -2
  PRINT "|bw="; NEURAL.DENSEBACKWARD(h, g, 2, gx); ":"; gx(0,0); ","; gx(0,1); ","; gx(1,0); ","; gx(1,1);
  PRINT "|st="; NEURAL.DENSESTEP(h, 0, 0.1); ":";
  PRINT NEURAL.DENSEPARAMS(h, W, b); "=";
  PRINT FORMAT(W(0), "#.0"); ","; FORMAT(W(1), "#.0"); ":"; FORMAT(b(0), "#.0");

  ' una matrice 2-D funziona anche con lo strato denso in inferenza (y = x·Wᵀ + b)
  NEURAL.DENSEFORWARD(h, x, 2, y)
  PRINT "|y2="; FORMAT(y(0,0), "#.00"); ","; FORMAT(y(1,0), "#.00");
  PRINT "|f="; NEURAL.DENSEFREE(h)
END SUB

' NEURAL (FFI) — scaler adattivo (gn_norm), gather/block (gn_ops), apply/absmax (gn_core).
' EXPECT: n= 7201 : 0 , 0 : 2 : 0 |ap= 0 =1.0,1.0|dn= 0 =3.0,4.0|zs= 0 =0.7071,0.7071|st= 0 =3.0,4.0,2.0,3.0,1.4142,1.4142|nf= 0 |ga= 0 =5.0,6.0,1.0,2.0|sl= 0 =3.0,4.0,5.0,6.0|cb= 0 =5.0,6.0,8.0,9.0|ap2= 0 =2.0,5.0,10.0|mx=5.0|sm= 0 =0.2689,0.7311|smt= 0 =0.1192,0.8808|lut= 0 =8.0,10.0|f=0

' Funzione applicata elemento per elemento (MATAPPLY).

SUB UnaNormOp()
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
  DIM z2(1) AS DOUBLE, so(1) AS DOUBLE
  z2(0)=0 : z2(1)=1
  PRINT "|sm="; NEURAL.SOFTMAXTEMP(so, z2, 1, 2, 1.0); "="; FORMAT(so(0), "#.0000"); ","; FORMAT(so(1), "#.0000");
  PRINT "|smt="; NEURAL.SOFTMAXTEMP(so, z2, 1, 2, 0.5); "="; FORMAT(so(0), "#.0000"); ","; FORMAT(so(1), "#.0000");
  DIM lut(7) AS DOUBLE, lj(1) AS LONG, ly(1) AS DOUBLE
  DIM t AS LONG
  FOR t = 0 TO 7
    lut(t) = t + 1
  NEXT t
  lj(0)=1 : lj(1)=0
  ly(0)=0 : ly(1)=0
  PRINT "|lut="; NEURAL.SNNLUTFORWARD(lut, 2, lj, 2, 2, ly); "="; FORMAT(ly(0), "#.0"); ","; FORMAT(ly(1), "#.0");
  PRINT "|f=0"
END SUB
' NEURAL (FFI) — autodiff scalare (micrograd) e programmazione genetica (gn_gp).
' EXPECT: z= 7004 :8.0|b= 0 |g=4.0,2.0,1.0|w=9.0:6.0,-6.0|t=0.0000|f= 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 |dv=2.0,1.0000,-3.0,8.0,0.0|gp= 7101 : 9 :26.0: 3 , 3 , 3 , 3 , 3 , 3 , 3 , 3 , 2 ,|ev=6.0|gpf= 0

' Fitness del GP: somma dei codici del genoma (da massimizzare).

SUB UnGrafo()
  ' z = x*y + x con x=2, y=3 → dz/dx = y+1 = 4, dz/dy = x = 2
  DIM X AS LONG, Y AS LONG, M AS LONG, Z AS LONG
  X = NEURAL.AUTOVALUE(2)
  Y = NEURAL.AUTOVALUE(3)
  M = NEURAL.AUTOMUL(X, Y)
  Z = NEURAL.AUTOADD(M, X)
  PRINT "z="; Z; ":"; FORMAT(NEURAL.AUTODATA(Z), "#.0"); "|b="; NEURAL.AUTOBACKWARD(Z); "|";
  PRINT "g="; FORMAT(NEURAL.AUTOGRAD(X), "#.0"); ","; FORMAT(NEURAL.AUTOGRAD(Y), "#.0");
  PRINT ","; FORMAT(NEURAL.AUTOGRAD(M), "#.0"); "|";

  ' w = (x - y)^2 con x=5, y=2 → w = 9, dw/dx = 6, dw/dy = -6
  DIM X2 AS LONG, Y2 AS LONG, D AS LONG, W AS LONG
  X2 = NEURAL.AUTOVALUE(5)
  Y2 = NEURAL.AUTOVALUE(2)
  D = NEURAL.AUTOSUB(X2, Y2)
  W = NEURAL.AUTOMUL(D, D)
  NEURAL.AUTOBACKWARD(W)
  PRINT "w="; FORMAT(NEURAL.AUTODATA(W), "#.0"); ":"; FORMAT(NEURAL.AUTOGRAD(X2), "#.0"); ","; FORMAT(NEURAL.AUTOGRAD(Y2), "#.0"); "|";
  PRINT "t="; FORMAT(NEURAL.AUTODATA(NEURAL.AUTOTANH(NEURAL.AUTOVALUE(0))), "#.0000"); "|";
  PRINT "f="; NEURAL.AUTOFREE(Z); ","; NEURAL.AUTOFREE(M); ","; NEURAL.AUTOFREE(X); ","; NEURAL.AUTOFREE(Y);
  PRINT ","; NEURAL.AUTOFREE(W); ","; NEURAL.AUTOFREE(D); ","; NEURAL.AUTOFREE(X2); ","; NEURAL.AUTOFREE(Y2); "|";

  ' ── operazioni mancanti: DIV, EXP, NEG, POW, RELU ──
  DIM U AS LONG, V AS LONG, Q AS LONG, T AS LONG
  U = NEURAL.AUTOVALUE(2) : V = NEURAL.AUTOVALUE(4)
  Q = NEURAL.AUTODIV(V, U)
  PRINT "dv="; FORMAT(NEURAL.AUTODATA(Q), "#.0");
  NEURAL.AUTOFREE(Q)
  T = NEURAL.AUTOVALUE(0)
  Q = NEURAL.AUTOEXP(T)
  PRINT ","; FORMAT(NEURAL.AUTODATA(Q), "#.0000");
  NEURAL.AUTOFREE(Q) : NEURAL.AUTOFREE(T)
  T = NEURAL.AUTOVALUE(3)
  Q = NEURAL.AUTONEG(T)
  PRINT ","; FORMAT(NEURAL.AUTODATA(Q), "#.0");
  NEURAL.AUTOFREE(Q) : NEURAL.AUTOFREE(T)
  Q = NEURAL.AUTOPOW(U, 3)
  PRINT ","; FORMAT(NEURAL.AUTODATA(Q), "#.0");
  NEURAL.AUTOFREE(Q)
  T = NEURAL.AUTOVALUE(-1)
  Q = NEURAL.AUTORELU(T)
  PRINT ","; FORMAT(NEURAL.AUTODATA(Q), "#.0"); "|";
  NEURAL.AUTOFREE(Q) : NEURAL.AUTOFREE(T)
  NEURAL.AUTOFREE(U) : NEURAL.AUTOFREE(V)

  ' GP: alfabeto {1,2,3} con arita' {0,0,1}; massimizza la somma dei codici.
  DIM codes(2) AS LONG, arity(2) AS LONG, best(62) AS LONG, gfit(0) AS DOUBLE
  codes(0)=1 : codes(1)=2 : codes(2)=3
  arity(0)=0 : arity(1)=0 : arity(2)=1
  gh = NEURAL.GPINIT(codes, arity, 3)
  PRINT "gp="; gh; ":";
  DIM n AS LONG
  n = NEURAL.GPEVOLVE(gh, GpFit, best, gfit, 6, 5, 42, 2)
  PRINT n; ":"; FORMAT(gfit(0), "#.0"); ":";
  DIM i AS LONG
  FOR i = 0 TO n - 1
    PRINT best(i); ",";
  NEXT i
  PRINT "|ev="; FORMAT(NEURAL.GPEVAL(gh, codes, 3, GpFit), "#.0");
  PRINT "|gpf="; NEURAL.GPFREE(gh)
END SUB

' Entry point: esegue tutte le famiglie in sequenza.
SUB Main()
  UnaRicorrente()
  UnaCnn()
  UnPruneQuant()
  UnAlbero()
  UnaUtility()
  UnaAttivazione()
  UnaNormDrop()
  UnaMatrice()
  UnGrafo()
  UnaNormOp()
  PRINT "NEURAL coverage: superficie completa richiamata."
END SUB
