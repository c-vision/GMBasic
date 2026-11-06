' NEURAL (FFI) — SNN, BNN, PINN, detection (IoU/NMS/YOLO), KNN, metriche, memplan.
' EXPECT: snn=0.50,0.250,-0.125|lut= 2 |bnn= 2 , 1 : 0 = 5 , 3 , 3 ,-2.0,-32767.0|pinn=0.0,1.0,1.0,9.0,56.25,4.0,9.0|pn=3.0|iou=1.0|nms= 2 : 0 , 2 |yolo= 4001 : 0 =0.1154,0.1154,0.1538,0.2308|yf= 0 |mc= 0 :0.9,0.0,0.7|knn= 4101 : 0 , 1 ,1.00|kf= 0 |cm= 0 : 1 , 1 , 1 , 1 , 2 |acc=0.6667,0.6667|pr=0.7222:1.00,0.50,0.67|rc=0.6667:0.50,1.00|f1=0.6556:0.50,0.80|reg=1.3333,0.6667,-1.0|mem=30.0:20.0,0.0,0.0|f=0
IMPORT NEURAL

SUB Main()
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
  yh = NEURAL.YOLOBOXCFG(2, 3, 1, 1, 13, 13)
  PRINT "yolo="; yh; ":"; NEURAL.YOLOBOX(yh, 0, 0, 0, 0, yo); "=";
  PRINT FORMAT(yo(0), "#.0000"); ","; FORMAT(yo(1), "#.0000"); ","; FORMAT(yo(2), "#.0000"); ","; FORMAT(yo(3), "#.0000");
  PRINT "|yf="; NEURAL.YOLOBOXFREE(yh);

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
  kh = NEURAL.KNNINIT(Xtr, ytr, 6, 1, 2)
  xq(0)=2.1
  PRINT "|knn="; kh; ":"; NEURAL.KNNCLASSIFY(kh, 1, xq);
  xq(0)=10.2
  PRINT ","; NEURAL.KNNCLASSIFY(kh, 1, xq);
  Xte(0)=0.1 : Xte(1)=10.9 : yte(0)=0 : yte(1)=1
  PRINT ","; FORMAT(NEURAL.KNNACCURACY(kh, 1, Xte, yte, 2), "#.00");
  PRINT "|kf="; NEURAL.KNNFREE(kh);

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

  DIM ms(2) AS DOUBLE, mb(2) AS LONG, md(2) AS LONG, mo(2) AS DOUBLE
  ms(0)=10 : ms(1)=20 : ms(2)=5
  mb(0)=0 : mb(1)=0 : mb(2)=2
  md(0)=1 : md(1)=1 : md(2)=3
  PRINT "mem="; FORMAT(NEURAL.MEMPLAN(ms, mb, md, 3, mo), "#.0"); ":";
  PRINT FORMAT(mo(0), "#.0"); ","; FORMAT(mo(1), "#.0"); ","; FORMAT(mo(2), "#.0"); "|";
  PRINT "f=0"
END SUB
