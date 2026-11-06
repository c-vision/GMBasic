' NEURAL (FFI) — CART (regressione/classificazione), Random Forest, AUC, AdaBoost.
' EXPECT: tr= 3001 : 0 ,10.0,20.0|tc= 3001 : 0 , 0 , 1 |fr= 3101 : 0 ,10.0,20.0|fc= 3101 : 0 , 0 , 1 |auc=0.75|ab= 3201 : 0 ,-1 , 1 |f= 0
IMPORT NEURAL

SUB Main()
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
