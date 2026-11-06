' NEURAL (FFI) — attivazioni/gradienti (gn_act), strato denso addestrabile, perdite.
' EXPECT: g=0.25,1.0,0.0|silu=0.7311,0.9277|elu=-0.6321,0.3679,-0.02,0.50|crelu= 0 :0.0,2.0,1.0,0.0|d= 5001 : 0 , 0 =1.0,2.0,4.0,3.0,4.0,8.0|b= 0 =1.0,0.0,0.0,1.0|s= 0 : 0 =0.90,-0.20,-0.30,0.60,1.00,1.00:-0.10,-0.10,1.00|zf= 0 |df= 0 |mse= 0 :0.0400,0.20,-0.20|mae= 0 :0.2000,0.50,-0.50|ce= 0 :0.2231,0.20,-0.20|bce= 0 :0.2231,0.6250,-0.6250|f=0
IMPORT NEURAL

SUB Main()
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
  PRINT "f=0"
END SUB
