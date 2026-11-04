' ORTOOLS (FFI) — meta/util/dp: MiniTest §4.1 della 2ª fascia.
' METASA, METATABU, METAGA, METAPSO, METAES, METAARS, METANSGA2, QUEUEMMC, LITTLESLAW,
' EPQ, NEWSVENDOR, SAFETYSTOCK, TOPSIS, MDPPOLICY, DESMM1, CHANCECONSTRAINT, DPEQUIPMENT.
' Obiettivo delle meta: f(x) = (x0-1)^2 + (x1+2)^2, minimo 0 in (1,-2), box [-5,5].
' EXPECT: sa=-1 |tabu=-1 |ga=-1 |pso=-1 |es=-1 |ars=-1 |nsga= 9 :-1 -1||mmc=0.667:-1 -1 |little=6.0|epq=316.23|news=108.61|ss=16.45|topsis= 1 :0.000,0.500,1.000|mdp= 100 :0.01.0|des=0.631:-1 |chance=2.33:1.645|dpeq=25.0: 0 -1 -1 -1 -1
IMPORT OPTIMIZE

FUNCTION F(x() AS DOUBLE) AS DOUBLE
  RETURN (x(0) - 1.0) * (x(0) - 1.0) + (x(1) + 2.0) * (x(1) + 2.0)
END FUNCTION

FUNCTION G1(x() AS DOUBLE) AS DOUBLE
  RETURN x(0) * x(0) + x(1) * x(1)
END FUNCTION

FUNCTION G2(x() AS DOUBLE) AS DOUBLE
  RETURN (x(0) - 2.0) * (x(0) - 2.0) + (x(1) - 2.0) * (x(1) - 2.0)
END FUNCTION

SUB Main()
  DIM lo(1) AS DOUBLE
  DIM hi(1) AS DOUBLE
  lo(0)=-5 : lo(1)=-5
  hi(0)=5 : hi(1)=5
  DIM x(1) AS DOUBLE

  ' ── Meta euristiche (deterministiche: nessun seed di tempo) ───────────────────────
  x(0)=4 : x(1)=4
  PRINT "sa="; (OPTIMIZE.METASA(F, x, 2, lo, hi, 400, 1.0, 0.99) < 1.0);
  x(0)=4 : x(1)=4
  PRINT "|tabu="; (OPTIMIZE.METATABU(F, x, 2, lo, hi, 300, 7) < 1.0);
  x(0)=4 : x(1)=4
  PRINT "|ga="; (OPTIMIZE.METAGA(F, x, 2, lo, hi, 24, 60) < 1.0);
  x(0)=4 : x(1)=4
  PRINT "|pso="; (OPTIMIZE.METAPSO(F, x, 2, lo, hi, 20, 150) < 1.0);
  x(0)=4 : x(1)=4
  PRINT "|es="; (OPTIMIZE.METAES(F, x, 2, 0.5, 0.99, 20, 200) < 1.0);
  x(0)=4 : x(1)=4
  PRINT "|ars="; (OPTIMIZE.METAARS(F, x, 2, 0.5, 0.99, 5, 20, 200) < 1.0);

  DIM out(39) AS DOUBLE           ' pareto[pop*d] | fv1[pop] | fv2[pop], pop=10, d=2
  r = OPTIMIZE.METANSGA2(G1, G2, 2, lo, hi, 10, 20, out)
  PRINT "|nsga="; r; ":"; (out(20) >= 0); (out(30) >= 0)

  ' ── Code e inventario ────────────────────────────────────────────────────────────
  DIM q(3) AS DOUBLE
  PRINT "|mmc="; FORMAT(OPTIMIZE.QUEUEMMC(2.0, 1.5, 2, q), "#.000"); ":"; (q(0) > 0); (q(1) > 0);
  PRINT "|little="; FORMAT(OPTIMIZE.LITTLESLAW(2.0, 3.0), "#.0");
  PRINT "|epq="; FORMAT(OPTIMIZE.EPQ(1000.0, 50.0, 2.0, 2000.0), "#.00");
  PRINT "|news="; FORMAT(OPTIMIZE.NEWSVENDOR(10.0, 5.0, 100.0, 20.0), "#.00");
  PRINT "|ss="; FORMAT(OPTIMIZE.SAFETYSTOCK(1.645, 10.0), "#.00");

  ' ── MCDM / MDP / simulazione / chance constraint ─────────────────────────────────
  DIM MAT(5) AS DOUBLE
  MAT(0)=1 : MAT(1)=9
  MAT(2)=2 : MAT(3)=8
  MAT(4)=3 : MAT(5)=7
  DIM wgt(1) AS DOUBLE
  wgt(0)=0.5 : wgt(1)=0.5
  DIM ben(1) AS LONG
  ben(0)=1 : ben(1)=0
  DIM sc(2) AS DOUBLE
  r = OPTIMIZE.TOPSIS(MAT, 3, 2, wgt, ben, sc)
  PRINT "|topsis="; r; ":"; FORMAT(sc(0), "#.000"); ","; FORMAT(sc(1), "#.000"); ","; FORMAT(sc(2), "#.000");

  ' MDP a 2 stati / 2 azioni: azione 0 porta in s0, azione 1 in s1
  DIM PR(7) AS DOUBLE
  PR(0)=1 : PR(1)=0     ' s0,a0 -> s0
  PR(2)=0 : PR(3)=1     ' s0,a1 -> s1
  PR(4)=1 : PR(5)=0     ' s1,a0 -> s0
  PR(6)=0 : PR(7)=1     ' s1,a1 -> s1
  DIM RW(3) AS DOUBLE
  RW(0)=1 : RW(1)=0 : RW(2)=0 : RW(3)=1
  DIM mo(3) AS DOUBLE
  PRINT "|mdp="; OPTIMIZE.MDPPOLICY(PR, RW, 2, 2, 0.9, 100, mo); ":";
  PRINT FORMAT(mo(2), "#.0"); FORMAT(mo(3), "#.0");

  DIM dq(0) AS DOUBLE
  PRINT "|des="; FORMAT(OPTIMIZE.DESMM1(2.0, 3.0, 800, dq), "#.000"); ":"; (dq(0) > 0.5);

  DIM sig(1) AS DOUBLE
  DIM xv(1) AS DOUBLE
  sig(0)=1 : sig(1)=1
  xv(0)=1 : xv(1)=1
  ' alpha = coda (0.05 -> z = 1.645); il ritorno e' il valore della chance constraint
  DIM cc(0) AS DOUBLE
  PRINT "|chance="; FORMAT(OPTIMIZE.CHANCECONSTRAINT(sig, xv, 2, 0.05, cc), "#.00"); ":";
  PRINT FORMAT(cc(0), "#.000");

  ' ── DP: sostituzione attrezzature (5 anni, vita massima 3) ───────────────────────
  DIM opc(2) AS DOUBLE
  DIM res(2) AS DOUBLE
  opc(0)=5 : opc(1)=8 : opc(2)=12
  res(0)=50 : res(1)=30 : res(2)=10
  DIM sched(4) AS LONG
  PRINT "|dpeq="; FORMAT(OPTIMIZE.DPEQUIPMENT(5, 100.0, 3, opc, res, sched), "#.0"); ":";
  PRINT sched(0); sched(1); sched(2); sched(3); sched(4)
  PRINT ""
END SUB
