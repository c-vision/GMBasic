' ORTOOLS (FFI) — scheduling: MiniTest §4.1 della 2ª fascia.
' SCHEDWSPT, SCHEDLAWLER, FLOWSHOPNEH, FLOWSHOPJOHNSON3, FLOWSHOPCDS, PROJECTPERT,
' PROJECTRCPSP, JOBSHOP.
' EXPECT: wspt=15.0: 1 : 0  1  2 |lawler=3.0: 1 |neh=10.0: 1 |j3=12.0: 1 |cds=10.0: 1 |pert=7.0:5.0:0.000|rcpsp=6.0: 1  3  5 |jshop=6.0: 0  3  0  3
IMPORT OPTIMIZE

FUNCTION IsPerm(t() AS LONG, n AS LONG) AS LONG
  DIM visto(63) AS LONG
  FOR i = 0 TO n - 1
    v = t(i)
    IF v < 0 OR v > n - 1 THEN RETURN 0
    IF visto(v) <> 0 THEN RETURN 0
    visto(v) = 1
  NEXT i
  RETURN 1
END FUNCTION

SUB Main()
  ' ── 1||ΣwC (WSPT) e 1||Lmax (Lawler) ─────────────────────────────────────────────
  DIM p(2) AS DOUBLE
  p(0)=1 : p(1)=2 : p(2)=3
  DIM w(2) AS DOUBLE
  w(0)=3 : w(1)=2 : w(2)=1
  DIM ord(2) AS LONG
  PRINT "wspt="; FORMAT(OPTIMIZE.SCHEDWSPT(p, w, 3, ord), "#.0"); ":"; IsPerm(ord, 3); ":";
  PRINT ord(0); ord(1); ord(2);

  DIM due(2) AS DOUBLE
  due(0)=3 : due(1)=3 : due(2)=3
  DIM ord2(2) AS LONG
  PRINT "|lawler="; FORMAT(OPTIMIZE.SCHEDLAWLER(p, due, 3, ord2), "#.0"); ":"; IsPerm(ord2, 3);

  ' ── Flow shop: NEH / Johnson3 / CDS ──────────────────────────────────────────────
  ' 3 job x 2 macchine, p(job*mac): j0=(2,3) j1=(4,1) j2=(3,2)
  DIM fp(5) AS DOUBLE
  fp(0)=2 : fp(1)=3
  fp(2)=4 : fp(3)=1
  fp(4)=3 : fp(5)=2
  DIM seq(2) AS LONG
  PRINT "|neh="; FORMAT(OPTIMIZE.FLOWSHOPNEH(fp, 3, 2, seq), "#.0"); ":"; IsPerm(seq, 3);

  DIM q1(2) AS DOUBLE
  DIM q2(2) AS DOUBLE
  DIM q3(2) AS DOUBLE
  q1(0)=2 : q1(1)=4 : q1(2)=3
  q2(0)=3 : q2(1)=1 : q2(2)=2
  q3(0)=2 : q3(1)=3 : q3(2)=1
  DIM seq2(2) AS LONG
  PRINT "|j3="; FORMAT(OPTIMIZE.FLOWSHOPJOHNSON3(q1, q2, q3, 3, seq2), "#.0"); ":"; IsPerm(seq2, 3);

  DIM seq3(2) AS LONG
  PRINT "|cds="; FORMAT(OPTIMIZE.FLOWSHOPCDS(fp, 3, 2, seq3), "#.0"); ":"; IsPerm(seq3, 3);

  ' ── PERT: catena A->B->C, tempi (to,tm,tp) ───────────────────────────────────────
  DIM to(2) AS DOUBLE
  DIM tm(2) AS DOUBLE
  DIM tp(2) AS DOUBLE
  to(0)=1 : to(1)=2 : to(2)=1
  tm(0)=2 : tm(1)=3 : tm(2)=2
  tp(0)=3 : tp(1)=4 : tp(2)=3
  DIM preds(2) AS LONG
  preds(0)=-1 : preds(1)=0 : preds(2)=1
  DIM est(2) AS DOUBLE
  DIM var(2) AS DOUBLE
  PRINT "|pert="; FORMAT(OPTIMIZE.PROJECTPERT(to, tm, tp, preds, 3, 1, est, var), "#.0"); ":";
  PRINT FORMAT(est(2), "#.0"); ":"; FORMAT(var(2), "#.000");

  ' ── RCPSP: 3 attività in catena, 1 risorsa con capacità 2 ────────────────────────
  DIM dur(2) AS DOUBLE
  dur(0)=2 : dur(1)=2 : dur(2)=1
  DIM dmd(2) AS DOUBLE
  dmd(0)=1 : dmd(1)=1 : dmd(2)=1
  DIM cap(0) AS DOUBLE
  cap(0)=2
  DIM st(2) AS LONG
  PRINT "|rcpsp="; FORMAT(OPTIMIZE.PROJECTRCPSP(dur, preds, 3, 1, dmd, cap, 1, st), "#.0"); ":";
  PRINT st(0); st(1); st(2);

  ' ── Job shop: 2 job x 2 macchine ─────────────────────────────────────────────────
  DIM route(3) AS LONG
  route(0)=0 : route(1)=1
  route(2)=1 : route(3)=0
  DIM jd(3) AS DOUBLE
  jd(0)=2 : jd(1)=3
  jd(2)=3 : jd(3)=1
  DIM js(3) AS LONG
  PRINT "|jshop="; FORMAT(OPTIMIZE.JOBSHOP(route, jd, 2, 2, js), "#.0"); ":";
  PRINT js(0); js(1); js(2); js(3)
  PRINT ""
END SUB
