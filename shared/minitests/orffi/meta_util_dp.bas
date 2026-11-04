' OPTIMIZE (FFI or) — meta-euristiche, OR-util, programmazione dinamica.
' EXPECT: de=-1 |cmaes=-1 |hc=-1 |tenure= 10 |aco=4.00|q=0.50:1.00:0.50|eoq=223.61|ahp= 1 :0.667|mdp=10.0|cvar=-1 |mc=0.50|bst=1.0|series=0.50
IMPORT OPTIMIZE

FUNCTION Sphere(x() AS DOUBLE) AS DOUBLE
  RETURN (x(0) - 2)^2 + (x(1) + 1)^2
END FUNCTION

FUNCTION Identity(x() AS DOUBLE) AS DOUBLE
  RETURN x(0)
END FUNCTION

SUB Main()
  DIM x(1) AS DOUBLE
  DIM lo(1) AS DOUBLE
  DIM hi(1) AS DOUBLE
  lo(0)=-5 : lo(1)=-5
  hi(0)=5 : hi(1)=5

  x(0)=0 : x(1)=0
  v = OPTIMIZE.METADE(Sphere, x, 2, lo, hi, 40, 120)
  PRINT "de="; (v < 0.01);

  x(0)=0 : x(1)=0
  v = OPTIMIZE.METACMAES(Sphere, x, 2, lo, hi, 120)
  PRINT "|cmaes="; (v < 0.05);

  x(0)=0 : x(1)=0
  v = OPTIMIZE.METAHILLCLIMB(Sphere, x, 2, lo, hi, 300)
  PRINT "|hc="; (v < 0.2);

  PRINT "|tenure="; OPTIMIZE.TABUTENURE(1, 5, 10);

  ' ACO TSP sul quadrato (lato 1)
  DIM d(15) AS DOUBLE
  DIM px(3) AS DOUBLE
  DIM py(3) AS DOUBLE
  px(0)=0 : px(1)=1 : px(2)=1 : px(3)=0
  py(0)=0 : py(1)=0 : py(2)=1 : py(3)=1
  FOR i = 0 TO 3
    FOR j = 0 TO 3
      d(i*4+j) = SQR((px(i)-px(j))^2 + (py(i)-py(j))^2)
    NEXT j
  NEXT i
  DIM tour(3) AS LONG
  PRINT "|aco="; FORMAT(OPTIMIZE.METAACOTSP(d, 4, tour, 10, 30), "#.00");

  ' coda M/M/1: lambda 0.5, mu 1 -> rho 0.5, L 1, Lq 0.5
  DIM q(3) AS DOUBLE
  PRINT "|q="; FORMAT(OPTIMIZE.QUEUEMM1(0.5, 1.0, q), "#.00"); ":"; FORMAT(q(0), "#.00"); ":"; FORMAT(q(1), "#.00");

  PRINT "|eoq="; FORMAT(OPTIMIZE.EOQ(1000.0, 50.0, 2.0), "#.00");

  ' AHP 2x2 reciproca
  DIM pw(3) AS DOUBLE
  pw(0)=1 : pw(1)=2 : pw(2)=0.5 : pw(3)=1
  DIM wt(1) AS DOUBLE
  DIM cons(0) AS DOUBLE
  ok = OPTIMIZE.AHP(pw, 2, wt, cons)
  PRINT "|ahp="; ok; ":"; FORMAT(wt(0), "#.000");

  ' MDP deterministico: V0 = 1/(1-0.9) = 10
  DIM tp(3) AS DOUBLE
  DIM rew(1) AS DOUBLE
  tp(0)=1 : tp(3)=1
  rew(0)=1 : rew(1)=0
  DIM mdp(3) AS DOUBLE
  ok = OPTIMIZE.MDPVALUE(tp, rew, 2, 1, 0.9, 200, mdp)
  PRINT "|mdp="; FORMAT(mdp(0), "#.0");

  ' CVaR
  DIM rets(3) AS DOUBLE
  rets(0)=-1 : rets(1)=0 : rets(2)=1 : rets(3)=2
  DIM vo(0) AS DOUBLE
  PRINT "|cvar="; (OPTIMIZE.CVAR(rets, 4, 0.75, vo) > 0);

  ' Monte Carlo: media di x su [0,1] = 0.5
  DIM mlo(0) AS DOUBLE
  DIM mhi(0) AS DOUBLE
  DIM mo(0) AS DOUBLE
  mlo(0)=0 : mhi(0)=1
  PRINT "|mc="; FORMAT(OPTIMIZE.MONTECARLO(Identity, 1, mlo, mhi, 20000, mo), "#.00");

  ' DP: optimal BST con una chiave p=1 -> costo 1
  DIM bp(0) AS DOUBLE
  bp(0)=1
  DIM root(0) AS LONG
  PRINT "|bst="; FORMAT(OPTIMIZE.DPOPTIMALBST(bp, 1, root), "#.0");

  ' DP: serie al meglio di 1, p=0.5 -> 0.5
  DIM ih(0) AS LONG
  ih(0)=1
  PRINT "|series="; FORMAT(OPTIMIZE.DPSPORTSSERIES(1, 1, 0.5, 0.5, ih), "#.00")
  PRINT ""
END SUB
