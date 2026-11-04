REM FILE: 10-OrTools.bas — pacchetto `or` (FFI): TUTTE le 64 funzioni di ORTOOLS di 2ª fascia.
REM Scenario: una giornata di pianificazione — approvvigionamento (pack/assign), copertura del
REM personale (cover/cp), giri di consegna (routing), sequenziamento (sched), scelte di
REM ottimizzazione (meta/util/dp). Istanze piccole: ogni riga stampa un valore di controllo
REM deterministico, identico sui 4 backend (nessun seed di tempo).
IMPORT OPTIMIZE

FUNCTION Fmet(x() AS DOUBLE) AS DOUBLE
  RETURN (x(0) - 1.0) * (x(0) - 1.0) + (x(1) + 2.0) * (x(1) + 2.0)
END FUNCTION
FUNCTION G1(x() AS DOUBLE) AS DOUBLE
  RETURN x(0) * x(0) + x(1) * x(1)
END FUNCTION
FUNCTION G2(x() AS DOUBLE) AS DOUBLE
  RETURN (x(0) - 2.0) * (x(0) - 2.0) + (x(1) - 2.0) * (x(1) - 2.0)
END FUNCTION

SUB Main()
  ' ═══ 1. APPROVVIGIONAMENTO: knapsack, subset sum, cutting stock, 3D, assegnamenti ═══
  DIM w(2) AS DOUBLE
  DIM v(2) AS DOUBLE
  w(0)=10 : w(1)=20 : w(2)=30
  v(0)=60 : v(1)=100 : v(2)=120
  DIM xf(2) AS DOUBLE
  PRINT "pack: frac="; FORMAT(OPTIMIZE.KNAPSACKFRACTIONAL(w, v, 3, 50.0, xf), "#.0");
  DIM t01(2) AS LONG
  PRINT "|mitm="; FORMAT(OPTIMIZE.KNAPSACK01MITM(w, v, 3, 50.0, t01), "#.0");
  DIM wu(1) AS DOUBLE
  DIM vu(1) AS DOUBLE
  wu(0)=5 : wu(1)=3
  vu(0)=10 : vu(1)=7
  DIM cu(1) AS LONG
  PRINT "|unb="; FORMAT(OPTIMIZE.KNAPSACKUNBOUNDED(wu, vu, 2, 11, cu), "#.0");

  DIM items(5) AS DOUBLE
  items(0)=3 : items(1)=34 : items(2)=4 : items(3)=12 : items(4)=5 : items(5)=2
  DIM take(5) AS LONG
  PRINT "|subset="; OPTIMIZE.SUBSETSUM(items, 6, 9.0, take);
  DIM pc(2) AS DOUBLE
  DIM pat(2) AS LONG
  pc(0)=5 : pc(1)=7 : pc(2)=9
  PRINT "|cut="; OPTIMIZE.CUTTINGSTOCK(pc, 3, 20.0, pat);
  DIM it3(8) AS DOUBLE
  FOR i = 0 TO 2 : it3(i*3)=2 : it3(i*3+1)=2 : it3(i*3+2)=2 : NEXT i
  DIM bo(2) AS LONG
  PRINT "|bp3d="; OPTIMIZE.BINPACKING3D(it3, 3, 3.0, 3.0, 3.0, bo)

  DIM cau(5) AS DOUBLE
  DIM aa(1) AS LONG
  cau(0)=1 : cau(1)=2 : cau(2)=3 : cau(3)=4 : cau(4)=1 : cau(5)=2
  PRINT "assign: auction="; FORMAT(OPTIMIZE.ASSIGNAUCTION(cau, 2, 3, aa, 0.01), "#.0");
  DIM cb(3) AS DOUBLE
  DIM ab(1) AS LONG
  cb(0)=1 : cb(1)=4 : cb(2)=4 : cb(3)=1
  PRINT "|bottleneck="; FORMAT(OPTIMIZE.ASSIGNBOTTLENECK(cb, 2, ab), "#.0");
  DIM pm(3) AS LONG
  DIM pw(3) AS LONG
  DIM mm(1) AS LONG
  pm(0)=0 : pm(1)=1 : pm(2)=1 : pm(3)=0
  pw(0)=1 : pw(1)=0 : pw(2)=0 : pw(3)=1
  PRINT "|stable="; OPTIMIZE.STABLEMARRIAGE(pm, pw, 2, mm);

  DIM sup(1) AS DOUBLE
  DIM dem(1) AS DOUBLE
  DIM tc(3) AS DOUBLE
  DIM xw(3) AS DOUBLE
  DIM xs(3) AS DOUBLE
  DIM xm(3) AS DOUBLE
  sup(0)=10 : sup(1)=10
  dem(0)=10 : dem(1)=10
  tc(0)=1 : tc(1)=2 : tc(2)=3 : tc(3)=1
  PRINT "transport: nw="; FORMAT(OPTIMIZE.TRANSPORTNORTHWEST(sup, dem, tc, 2, 2, xw), "#.0");
  PRINT "|leastcost="; FORMAT(OPTIMIZE.TRANSPORTLEASTCOST(sup, dem, tc, 2, 2, xs), "#.0");
  PRINT "|modi="; FORMAT(OPTIMIZE.TRANSPORTMODI(sup, dem, tc, 2, 2, xm, 50), "#.0")

  DIM Aq(8) AS DOUBLE
  DIM Bq(8) AS DOUBLE
  DIM pq(2) AS LONG
  FOR i = 0 TO 8
    Aq(i) = (i MOD 3) + 1
    IF i MOD 4 = 0 THEN
      Bq(i) = 0
    ELSE
      Bq(i) = 1
    END IF
  NEXT i
  pq(0)=0 : pq(1)=1 : pq(2)=2
  PRINT "qap: " ; FORMAT(OPTIMIZE.ASSIGNQAPTABU(Aq, Bq, 3, pq, 200, 7), "#.0")

  ' ═══ 2. COPERTURA DEL PERSONALE: set cover (5 varianti), max coverage, p-median, … ═══
  DIM E(5) AS LONG
  DIM C(1) AS DOUBLE
  E(0)=1 : E(1)=0
  E(2)=0 : E(3)=1
  E(4)=0 : E(5)=1
  C(0)=1 : C(1)=3
  DIM ch(1) AS LONG
  DIM unc(0) AS LONG
  ch(0)=1 : ch(1)=1
  PRINT "cover: soft="; FORMAT(OPTIMIZE.SETCOVERSOFT(E, C, 3, 2, ch, 10.0, unc), "#.0");
  DIM y(2) AS DOUBLE
  DIM cs(2) AS LONG
  y(0)=5 : y(1)=3 : y(2)=-1
  PRINT "|pricing="; FORMAT(OPTIMIZE.SETCOVERPRICING(y, 3, 1.0, 2.0, cs), "#.0");
  DIM c1(1) AS LONG
  PRINT "|sa="; FORMAT(OPTIMIZE.SETCOVERSA(E, C, 3, 2, 10.0, 200, 1, c1), "#.0");
  DIM gch(1) AS LONG
  DIM ngch(0) AS LONG
  cg = OPTIMIZE.SETCOVER(E, C, 3, 2, gch, ngch)
  DIM c2(1) AS LONG
  DIM nc(0) AS LONG
  c2(0)=gch(0) : c2(1)=gch(1) : nc(0)=ngch(0)
  PRINT "|local="; FORMAT(OPTIMIZE.SETCOVERLOCAL(E, C, 3, 2, c2, nc, 50), "#.0");
  DIM c3(1) AS LONG
  DIM nc3(0) AS LONG
  PRINT "|grasp="; FORMAT(OPTIMIZE.SETCOVERGRASP(E, C, 3, 2, 0.3, 50, c3, nc3), "#.0");
  DIM al(2) AS DOUBLE
  DIM c4(1) AS LONG
  DIM nc4(0) AS LONG
  al(0)=2 : al(1)=0.2 : al(2)=0.8
  PRINT "|rgrasp="; FORMAT(OPTIMIZE.SETCOVERRGRASP(E, C, 3, 2, al, 50, c4, nc4), "#.0")
  DIM mc(1) AS LONG
  DIM nmc(0) AS LONG
  PRINT "maxcov="; OPTIMIZE.MAXCOVERAGE(E, 3, 2, 1, mc, nmc)

  DIM dd(15) AS DOUBLE
  DIM med(1) AS LONG
  DIM asg(3) AS LONG
  FOR i = 0 TO 3
    FOR j = 0 TO 3
      dd(i*4+j) = ABS(i-j)
    NEXT j
  NEXT i
  PRINT "pmedian="; FORMAT(OPTIMIZE.PMEDIAN(dd, 4, 2, med, asg, 50), "#.0");
  DIM ed(5) AS LONG
  DIM cov(2) AS LONG
  ed(0)=0 : ed(1)=1
  ed(2)=1 : ed(3)=2
  ed(4)=0 : ed(5)=2
  PRINT "|vcover="; OPTIMIZE.VERTEXCOVER(ed, 3, 3, cov);
  DIM hc(1) AS LONG
  DIM nhc(0) AS LONG
  PRINT "|hitting="; OPTIMIZE.HITTINGSET(E, 3, 2, hc, nhc);
  DIM oc(1) AS DOUBLE
  DIM cc2(3) AS DOUBLE
  DIM op(1) AS LONG
  oc(0)=5 : oc(1)=6
  cc2(0)=1 : cc2(1)=9 : cc2(2)=9 : cc2(3)=1
  PRINT "|facility="; FORMAT(OPTIMIZE.FACILITYLOCATION(oc, cc2, 2, 2, op), "#.0")

  DIM dm2(3) AS LONG
  DIM alw(15) AS LONG
  DIM sol(1) AS LONG
  dm2(0)=0 : dm2(1)=1 : dm2(2)=0 : dm2(3)=1
  FOR i = 0 TO 15 : alw(i) = 0 : NEXT i
  alw((0*2+1)*4 + 0*2 + 0) = 1
  alw((0*2+1)*4 + 1*2 + 1) = 1
  PRINT "cp="; OPTIMIZE.CPBINARY(dm2, alw, 2, 2, sol); ":"; sol(0); sol(1)

  ' ═══ 3. GIRI DI CONSEGNA (TSP/VRP) ═══════════════════════════════════════════════
  DIM d(24) AS DOUBLE
  FOR i = 0 TO 4
    FOR j = 0 TO 4
      d(i*5+j) = ABS(i-j)
    NEXT j
  NEXT i
  DIM t(4) AS LONG
  FOR i = 0 TO 4 : t(i) = i : NEXT i
  PRINT "tsp: extra="; FORMAT(OPTIMIZE.TSPEXTRAMILEAGE(d, 5, t), "#.0");
  FOR i = 0 TO 4 : t(i) = i : NEXT i
  PRINT "|grasp="; FORMAT(OPTIMIZE.TSPGRASP(d, 5, t, 60), "#.0");
  FOR i = 0 TO 4 : t(i) = 4 - i : NEXT i
  PRINT "|vns="; FORMAT(OPTIMIZE.TSPVNS(d, 5, t, 60), "#.0");
  FOR i = 0 TO 4 : t(i) = 4 - i : NEXT i
  PRINT "|3opt="; FORMAT(OPTIMIZE.TSP3OPT(d, 5, t, 60), "#.0");
  FOR i = 0 TO 4 : t(i) = 4 - i : NEXT i
  PRINT "|sa="; FORMAT(OPTIMIZE.TSPSA(d, 5, t, 5.0, 0.99, 300), "#.0");
  FOR i = 0 TO 4 : t(i) = 4 - i : NEXT i
  PRINT "|tabu="; FORMAT(OPTIMIZE.TSPTABU(d, 5, t, 60, 1), "#.0");
  DIM duali(4) AS DOUBLE
  PRINT "|hk="; FORMAT(OPTIMIZE.TSPHKBOUND(d, 5, 50, duali), "#.0");
  FOR i = 0 TO 4 : t(i) = 0 : NEXT i
  PRINT "|dfj="; FORMAT(OPTIMIZE.TSPDFJ(d, 5, t, 10), "#.0");
  FOR i = 0 TO 4 : t(i) = i : NEXT i
  PRINT "|oropt="; FORMAT(OPTIMIZE.TSPOROPT(d, 5, t, 60), "#.0");
  FOR i = 0 TO 4 : t(i) = i : NEXT i
  PRINT "|ga="; FORMAT(OPTIMIZE.TSPGA(d, 5, t, 12, 30), "#.0")

  DIM dem5(3) AS DOUBLE
  DIM tw(11) AS DOUBLE
  DIM ro(3) AS LONG
  DIM tot(0) AS DOUBLE
  FOR i = 0 TO 3
    dem5(i) = 1
    tw(i) = 0
    tw(4+i) = 100
    tw(8+i) = 0
  NEXT i
  PRINT "vrp: tw="; OPTIMIZE.VRPTIMEWINDOWS(d, dem5, 4, 3.0, 0, tw, ro, tot); ":";
  PRINT FORMAT(tot(0), "#.0");
  DIM dem4(4) AS DOUBLE
  DIM ro2(4) AS LONG
  DIM tot2(0) AS DOUBLE
  FOR i = 0 TO 4 : dem4(i) = 1 : NEXT i
  PRINT "|mdvrp="; OPTIMIZE.VRPMULTIDEPOT(d, dem4, 5, 2, 3.0, ro2, tot2); ":";
  PRINT FORMAT(tot2(0), "#.0");
  DIM pk(4) AS LONG
  DIM dl(4) AS LONG
  DIM rt(3) AS LONG
  DIM tot3(0) AS DOUBLE
  pk(1)=1 : pk(3)=1
  dl(2)=1 : dl(4)=1
  PRINT "|pdp="; OPTIMIZE.VRPPICKUPDELIVERY(d, dem4, 2, 4.0, pk, dl, rt, tot3); ":";
  PRINT FORMAT(tot3(0), "#.0")

  ' ═══ 4. SEQUENZIAMENTO: 1 macchina, flow shop, progetto, job shop ═══════════════════
  DIM p(2) AS DOUBLE
  DIM wj(2) AS DOUBLE
  DIM ord(2) AS LONG
  p(0)=1 : p(1)=2 : p(2)=3
  wj(0)=3 : wj(1)=2 : wj(2)=1
  PRINT "sched: wspt="; FORMAT(OPTIMIZE.SCHEDWSPT(p, wj, 3, ord), "#.0");
  DIM due(2) AS DOUBLE
  DIM ord2(2) AS LONG
  due(0)=3 : due(1)=3 : due(2)=3
  PRINT "|lawler="; FORMAT(OPTIMIZE.SCHEDLAWLER(p, due, 3, ord2), "#.0");
  DIM fp(5) AS DOUBLE
  DIM seq(2) AS LONG
  fp(0)=2 : fp(1)=3
  fp(2)=4 : fp(3)=1
  fp(4)=3 : fp(5)=2
  PRINT "|neh="; FORMAT(OPTIMIZE.FLOWSHOPNEH(fp, 3, 2, seq), "#.0");
  DIM q1(2) AS DOUBLE
  DIM q2(2) AS DOUBLE
  DIM q3(2) AS DOUBLE
  DIM seq2(2) AS LONG
  DIM seq3(2) AS LONG
  q1(0)=2 : q1(1)=4 : q1(2)=3
  q2(0)=3 : q2(1)=1 : q2(2)=2
  q3(0)=2 : q3(1)=3 : q3(2)=1
  PRINT "|johnson3="; FORMAT(OPTIMIZE.FLOWSHOPJOHNSON3(q1, q2, q3, 3, seq2), "#.0");
  PRINT "|cds="; FORMAT(OPTIMIZE.FLOWSHOPCDS(fp, 3, 2, seq3), "#.0")

  DIM to(2) AS DOUBLE
  DIM tm(2) AS DOUBLE
  DIM tp(2) AS DOUBLE
  DIM preds(2) AS LONG
  DIM est(2) AS DOUBLE
  DIM varp(2) AS DOUBLE
  to(0)=1 : to(1)=2 : to(2)=1
  tm(0)=2 : tm(1)=3 : tm(2)=2
  tp(0)=3 : tp(1)=4 : tp(2)=3
  preds(0)=-1 : preds(1)=0 : preds(2)=1
  PRINT "pert="; FORMAT(OPTIMIZE.PROJECTPERT(to, tm, tp, preds, 3, 1, est, varp), "#.0");
  DIM dur(2) AS DOUBLE
  DIM dmd(2) AS DOUBLE
  DIM cap(0) AS DOUBLE
  DIM st(2) AS LONG
  dur(0)=2 : dur(1)=2 : dur(2)=1
  dmd(0)=1 : dmd(1)=1 : dmd(2)=1
  cap(0)=2
  PRINT "|rcpsp="; FORMAT(OPTIMIZE.PROJECTRCPSP(dur, preds, 3, 1, dmd, cap, 1, st), "#.0");
  DIM route(3) AS LONG
  DIM jd(3) AS DOUBLE
  DIM js(3) AS LONG
  route(0)=0 : route(1)=1
  route(2)=1 : route(3)=0
  jd(0)=2 : jd(1)=3
  jd(2)=3 : jd(3)=1
  PRINT "|jobshop="; FORMAT(OPTIMIZE.JOBSHOP(route, jd, 2, 2, js), "#.0")

  ' ═══ 5. OTTIMIZZAZIONE E DECISIONI: meta, code, inventario, MCDM, MDP, DP ══════════
  DIM lo(1) AS DOUBLE
  DIM hi(1) AS DOUBLE
  DIM xm2(1) AS DOUBLE
  lo(0)=-5 : lo(1)=-5
  hi(0)=5 : hi(1)=5
  xm2(0)=4 : xm2(1)=4
  PRINT "meta: sa="; (OPTIMIZE.METASA(Fmet, xm2, 2, lo, hi, 120, 1.0, 0.99) < 1.0);
  xm2(0)=4 : xm2(1)=4
  PRINT "|tabu="; (OPTIMIZE.METATABU(Fmet, xm2, 2, lo, hi, 120, 7) < 1.0);
  xm2(0)=4 : xm2(1)=4
  PRINT "|ga="; (OPTIMIZE.METAGA(Fmet, xm2, 2, lo, hi, 10, 20) < 1.0);
  xm2(0)=4 : xm2(1)=4
  PRINT "|pso="; (OPTIMIZE.METAPSO(Fmet, xm2, 2, lo, hi, 10, 40) < 1.0);
  xm2(0)=4 : xm2(1)=4
  PRINT "|es="; (OPTIMIZE.METAES(Fmet, xm2, 2, 0.5, 0.99, 10, 60) < 1.0);
  xm2(0)=4 : xm2(1)=4
  PRINT "|ars="; (OPTIMIZE.METAARS(Fmet, xm2, 2, 0.5, 0.99, 5, 10, 60) < 1.0);
  DIM pout(39) AS DOUBLE
  PRINT "|nsga="; OPTIMIZE.METANSGA2(G1, G2, 2, lo, hi, 10, 20, pout)

  DIM qq(3) AS DOUBLE
  PRINT "queue: mmc="; FORMAT(OPTIMIZE.QUEUEMMC(2.0, 1.5, 2, qq), "#.000");
  PRINT "|little="; FORMAT(OPTIMIZE.LITTLESLAW(2.0, 3.0), "#.0");
  PRINT "|des="; FORMAT(OPTIMIZE.DESMM1(2.0, 3.0, 300, qq), "#.000");
  PRINT "inv: epq="; FORMAT(OPTIMIZE.EPQ(1000.0, 50.0, 2.0, 2000.0), "#.00");
  PRINT "|news="; FORMAT(OPTIMIZE.NEWSVENDOR(10.0, 5.0, 100.0, 20.0), "#.00");
  PRINT "|ss="; FORMAT(OPTIMIZE.SAFETYSTOCK(1.645, 10.0), "#.00")

  DIM MAT(5) AS DOUBLE
  DIM wgt(1) AS DOUBLE
  DIM ben(1) AS LONG
  DIM sc(2) AS DOUBLE
  MAT(0)=1 : MAT(1)=9
  MAT(2)=2 : MAT(3)=8
  MAT(4)=3 : MAT(5)=7
  wgt(0)=0.5 : wgt(1)=0.5
  ben(0)=1 : ben(1)=0
  PRINT "mcdm: topsis="; OPTIMIZE.TOPSIS(MAT, 3, 2, wgt, ben, sc)
  DIM PR(7) AS DOUBLE
  DIM RW(3) AS DOUBLE
  DIM mo(3) AS DOUBLE
  PR(0)=1 : PR(1)=0 : PR(2)=0 : PR(3)=1
  PR(4)=1 : PR(5)=0 : PR(6)=0 : PR(7)=1
  RW(0)=1 : RW(1)=0 : RW(2)=0 : RW(3)=1
  PRINT "mdp="; OPTIMIZE.MDPPOLICY(PR, RW, 2, 2, 0.9, 100, mo)

  DIM sig(1) AS DOUBLE
  DIM xv(1) AS DOUBLE
  DIM ccv(0) AS DOUBLE
  sig(0)=1 : sig(1)=1
  xv(0)=1 : xv(1)=1
  PRINT "chance="; FORMAT(OPTIMIZE.CHANCECONSTRAINT(sig, xv, 2, 0.05, ccv), "#.00")

  DIM opc(2) AS DOUBLE
  DIM res(2) AS DOUBLE
  DIM sched(4) AS LONG
  opc(0)=5 : opc(1)=8 : opc(2)=12
  res(0)=50 : res(1)=30 : res(2)=10
  PRINT "dp: equipment="; FORMAT(OPTIMIZE.DPEQUIPMENT(5, 100.0, 3, opc, res, sched), "#.0")
  PRINT ""
END SUB
