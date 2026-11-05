REM FILE: 14-OrToolsCopertura.bas — pacchetto `or` (FFI): le 41 funzioni di PRIMA fascia che
REM non comparivano in nessun altro sample. Con 10-OrTools.bas (64 funzioni di 2ª fascia) il
REM pacchetto `ortools.inc` è coperto per intero: 106/106.
REM Le istanze sono le stesse dei MiniTest (già verdi su 4 backend): qui diventano un esempio
REM leggibile, organico per dominio.
IMPORT OPTIMIZE

FUNCTION Sfera(x() AS DOUBLE) AS DOUBLE
  RETURN (x(0) - 2) ^ 2 + (x(1) + 1) ^ 2
END FUNCTION

FUNCTION Ident(x() AS DOUBLE) AS DOUBLE
  RETURN x(0)
END FUNCTION

SUB Main()
  ' ═══ 1. PACK: bin packing 1D (euristica + lower bound) e zaino 0/1 ════════════════
  DIM it() = {4, 3, 3, 2}
  DIM binOf(4) AS LONG
  PRINT "pack: bins="; OPTIMIZE.BINPACKING(it, 4, 10.0, 4, binOf);
  PRINT " lb="; OPTIMIZE.BINPACKINGLB(it, 4, 10.0);
  DIM kw() = {1, 2, 3}
  DIM kv() = {6, 10, 12}
  DIM ktake(3) AS LONG
  PRINT "|knap01="; FORMAT(OPTIMIZE.KNAPSACK01(kw, kv, 3, 5, ktake), "#.0")

  ' ═══ 2. ASSIGN: Hungarian e trasporto (VAM) ═══════════════════════════════════════
  DIM acost() = {1, 2, 3, 4}
  DIM asg(2) AS LONG
  PRINT "assign: hungarian="; FORMAT(OPTIMIZE.ASSIGNHUNGARIAN(acost, 2, 2, asg), "#.0");
  DIM asup() = {10, 10}
  DIM adem() = {10, 10}
  DIM atc() = {1, 2, 3, 1}
  DIM aplan(4) AS DOUBLE
  PRINT "|vam="; FORMAT(OPTIMIZE.TRANSPORTVAM(asup, adem, atc, 2, 2, aplan), "#.0")

  ' ═══ 3. COPERTURA e CLUSTERING: p-center, k-means ═════════════════════════════════
  DIM pcd(16) AS DOUBLE
  DIM pmed(2) AS LONG
  FOR i = 0 TO 3
    FOR j = 0 TO 3
      pcd(i*4+j) = ABS(i-j)
    NEXT j
  NEXT i
  PRINT "cover: pcenter="; FORMAT(OPTIMIZE.PCENTER(pcd, 4, 2, pmed), "#.0");
  DIM kp() = {0, 1, 10, 11}
  DIM kcent(2) AS DOUBLE
  DIM klab(4) AS LONG
  PRINT "|kmeans="; FORMAT(OPTIMIZE.KMEANS(kp, 4, 1, 2, kcent, klab, 50), "#.0"); ":";
  PRINT FORMAT(kcent(0) + kcent(1), "#.0")

  ' ═══ 4. CP: SAT (DPLL) e all-different ══════════════════════════════════════════
  DIM scl(3) AS LONG
  DIM scs(1) AS LONG
  DIM slen(1) AS LONG
  DIM ssol(2) AS LONG
  scl(0)=1 : scl(1)=2 : scl(2)=-1 : scl(3)=2
  scs(0)=0 : scs(1)=2
  slen(0)=2 : slen(1)=2
  PRINT "cp: sat="; OPTIMIZE.SATDPLL(scl, scs, slen, 2, 2, ssol);
  DIM adom(8) AS LONG
  FOR i = 0 TO 8
    adom(i) = i MOD 3
  NEXT i
  DIM asol(3) AS LONG
  PRINT "|alldiff="; OPTIMIZE.CPALLDIFF(adom, 3, 3, asol)

  ' ═══ 5. ROUTING: euristiche TSP, bound, sottotour, VRP, arc routing ══════════════
  DIM d(15) AS DOUBLE
  DIM px(3) AS DOUBLE
  DIM py(3) AS DOUBLE
  px(0)=0 : px(1)=1 : px(2)=1 : px(3)=0
  py(0)=0 : py(1)=0 : py(2)=1 : py(3)=1
  FOR i = 0 TO 3
    FOR j = 0 TO 3
      d(i*4+j) = SQR((px(i) - px(j)) ^ 2 + (py(i) - py(j)) ^ 2)
    NEXT j
  NEXT i
  DIM tour(3) AS LONG
  FOR i = 0 TO 3 : tour(i) = i : NEXT i
  PRINT "tsp: nn="; FORMAT(OPTIMIZE.TSPNEAREST(d, 4, tour), "#.0");
  PRINT "|2opt="; FORMAT(OPTIMIZE.TSP2OPT(d, 4, tour, 50), "#.0");
  PRINT "|cost="; FORMAT(OPTIMIZE.TSPTOURCOST(d, 4, tour), "#.0");
  DIM t2(3) AS LONG
  FOR i = 0 TO 3 : t2(i) = i : NEXT i
  PRINT "|hk="; FORMAT(OPTIMIZE.TSPHELDKARP(d, 4, t2), "#.0");
  PRINT "|dbltree="; FORMAT(OPTIMIZE.TSPDOUBLETREE(d, 4, t2), "#.0");
  DIM edges(7) AS LONG
  PRINT "|1tree="; FORMAT(OPTIMIZE.TSPONETREE(d, 4, 0, edges), "#.0");
  DIM succ(2) AS LONG
  DIM comp(2) AS LONG
  DIM sizes(2) AS LONG
  succ(0)=1 : succ(1)=2 : succ(2)=0
  PRINT "|subtours="; OPTIMIZE.TSPSUBTOURS(succ, 3, comp, sizes);
  DIM vd(15) AS DOUBLE
  FOR i = 0 TO 3
    FOR j = 0 TO 3
      vd(i*4+j) = ABS(i-j)
    NEXT j
  NEXT i
  DIM vdem(2) AS DOUBLE
  DIM rof(2) AS LONG
  DIM vtot(0) AS DOUBLE
  vdem(0)=1 : vdem(1)=1 : vdem(2)=1
  PRINT "|vrp="; OPTIMIZE.VRPCLARKEWRIGHT(vd, vdem, 3, 3.0, 0, rof, vtot); ":";
  PRINT FORMAT(vtot(0), "#.0");
  DIM aw(8) AS DOUBLE
  DIM eul(10) AS LONG
  DIM elen(0) AS LONG
  aw(1)=1 : aw(5)=1 : aw(6)=1
  PRINT "|arc="; FORMAT(OPTIMIZE.ARCROUTING(aw, 3, eul, elen), "#.0"); ":"; elen(0)

  ' ═══ 6. SCHEDULING: 1 macchina, intervalli, flow shop, progetto, turni ═══════════
  DIM sp() = {3, 1, 2}
  DIM sdue() = {3, 2, 4}
  DIM sord(2) AS LONG
  PRINT "sched: spt="; FORMAT(OPTIMIZE.SCHEDSPT(sp, 3, sord), "#.0");
  PRINT "|edd="; FORMAT(OPTIMIZE.SCHEDEDD(sp, sdue, 3, sord), "#.0");
  PRINT "|moore="; OPTIMIZE.SCHEDMOORE(sp, sdue, 3, sord);
  DIM ist() = {0, 1, 0}
  DIM ien() = {1, 2, 2}
  DIM isel(2) AS LONG
  PRINT "|imax="; OPTIMIZE.INTERVALMAX(ist, ien, 3, isel);
  PRINT "|ipart="; OPTIMIZE.INTERVALPARTITION(ist, ien, 3);
  DIM j1() = {2, 1}
  DIM j2() = {1, 2}
  DIM jseq(1) AS LONG
  PRINT "|johnson="; FORMAT(OPTIMIZE.FLOWSHOPJOHNSON(j1, j2, 2, jseq), "#.0");
  DIM lpt() = {1, 2, 3}
  DIM lasg(2) AS LONG
  PRINT "|lpt="; FORMAT(OPTIMIZE.PARALLELLPT(lpt, 3, 2, lasg), "#.0");
  DIM cdur() = {1, 2, 3}
  DIM cpreds(5) AS LONG
  DIM cest(2) AS DOUBLE
  DIM clst(2) AS DOUBLE
  DIM cslack(2) AS DOUBLE
  FOR i = 0 TO 5
    cpreds(i) = -1
  NEXT i
  cpreds(4) = 0
  cpreds(5) = 1
  PRINT "|cpm="; FORMAT(OPTIMIZE.PROJECTCPM(cdur, cpreds, 3, 2, cest, clst, cslack), "#.0");
  DIM conf(8) AS LONG
  DIM slot(2) AS LONG
  conf(1)=1 : conf(2)=1
  conf(3)=1 : conf(5)=1
  conf(6)=1 : conf(7)=1
  PRINT "|tt="; OPTIMIZE.TIMETABLING(conf, 3, slot);
  DIM rd(1) AS LONG
  DIM rsched(3) AS LONG
  rd(0)=1 : rd(1)=1
  PRINT "|rost="; OPTIMIZE.ROSTERING(rd, 2, 2, 1, rsched)

  ' ═══ 7. META-EURISTICHE: hill climbing, DE, CMA-ES, ACO, tenure ══════════════════
  DIM mx(1) AS DOUBLE
  DIM mlo(1) AS DOUBLE
  DIM mhi(1) AS DOUBLE
  mlo(0)=-5 : mlo(1)=-5
  mhi(0)=5 : mhi(1)=5
  mx(0)=0 : mx(1)=0
  PRINT "meta: hc="; (OPTIMIZE.METAHILLCLIMB(Sfera, mx, 2, mlo, mhi, 300) < 0.2);
  mx(0)=0 : mx(1)=0
  PRINT "|de="; (OPTIMIZE.METADE(Sfera, mx, 2, mlo, mhi, 40, 120) < 0.01);
  mx(0)=0 : mx(1)=0
  PRINT "|cmaes="; (OPTIMIZE.METACMAES(Sfera, mx, 2, mlo, mhi, 120) < 0.05);
  DIM atour(3) AS LONG
  PRINT "|aco="; FORMAT(OPTIMIZE.METAACOTSP(d, 4, atour, 10, 30), "#.00");
  PRINT "|tenure="; OPTIMIZE.TABUTENURE(1, 5, 10)

  ' ═══ 8. OR-UTIL: code, inventario, MCDM, MDP, simulazione, CVaR ══════════════════
  DIM q(3) AS DOUBLE
  PRINT "util: mm1="; FORMAT(OPTIMIZE.QUEUEMM1(0.5, 1.0, q), "#.00"); ":";
  PRINT FORMAT(q(0), "#.00"); ":"; FORMAT(q(1), "#.00");
  PRINT "|eoq="; FORMAT(OPTIMIZE.EOQ(1000.0, 50.0, 2.0), "#.00");
  DIM pw(3) AS DOUBLE
  DIM wt(1) AS DOUBLE
  DIM cons(0) AS DOUBLE
  pw(0)=1 : pw(1)=2 : pw(2)=0.5 : pw(3)=1
  PRINT "|ahp="; OPTIMIZE.AHP(pw, 2, wt, cons); ":"; FORMAT(wt(0), "#.000");
  DIM tp(3) AS DOUBLE
  DIM rew(1) AS DOUBLE
  DIM mdp(3) AS DOUBLE
  tp(0)=1 : tp(3)=1
  rew(0)=1 : rew(1)=0
  PRINT "|mdp="; FORMAT(OPTIMIZE.MDPVALUE(tp, rew, 2, 1, 0.9, 200, mdp), "#.0");
  PRINT FORMAT(mdp(0), "#.0");
  DIM rets(3) AS DOUBLE
  DIM vo(0) AS DOUBLE
  rets(0)=-1 : rets(1)=0 : rets(2)=1 : rets(3)=2
  PRINT "|cvar="; (OPTIMIZE.CVAR(rets, 4, 0.75, vo) > 0);
  DIM xlo(0) AS DOUBLE
  DIM xhi(0) AS DOUBLE
  DIM xo(0) AS DOUBLE
  xlo(0)=0 : xhi(0)=1
  PRINT "|mc="; FORMAT(OPTIMIZE.MONTECARLO(Ident, 1, xlo, xhi, 20000, xo), "#.00")

  ' ═══ 9. DP: BST ottimo e serie sportiva ══════════════════════════════════════════
  DIM bp(0) AS DOUBLE
  DIM root(0) AS LONG
  bp(0)=1
  PRINT "dp: bst="; FORMAT(OPTIMIZE.DPOPTIMALBST(bp, 1, root), "#.0");
  DIM ih(0) AS LONG
  ih(0)=1
  PRINT "|series="; FORMAT(OPTIMIZE.DPSPORTSSERIES(1, 1, 0.5, 0.5, ih), "#.00")
  PRINT ""
END SUB
