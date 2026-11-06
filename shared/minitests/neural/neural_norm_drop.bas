' NEURAL (FFI) — dropout, BatchNorm, LayerNorm, ottimizzatore Chuck.
' EXPECT: off= 6001 : 0 =1.0,2.0,3.0,4.0|df= 0 |on= 6001 : 0 =0.00,4.00,6.00,0.00|db= 0 =0.00,2.00,2.00,0.00|df2= 0 |bn= 6101 : 0 =-1.0000,-1.0000,1.0000,1.0000|bp= 0 =1.0,0.0|bs= 0 : 0 =2.0,1.0|bb= 0 =0.0000,0.0000,0.0000,0.0000|bz= 0 |bf= 0 |ln= 6201 : 0 =-1.0000,1.0000|lp= 0 =1.0,0.0|ls= 0 : 0 =3.0,2.0|lb= 0 =-0.0000,0.0000|lz= 0 |lf= 0 |ck= 6301 : 0 |st= 0 =0.9253|cs= 0 =1.00,0.25,0.001,2.0,0.0|cr= 0 |cf= 0 |f=0
IMPORT NEURAL

SUB Main()
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
