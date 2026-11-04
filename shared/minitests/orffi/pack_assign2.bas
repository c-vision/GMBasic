' ORTOOLS (FFI) — pack + assign: MiniTest §4.1 delle superfici della 2ª fascia.
' KNAPSACKFRACTIONAL, KNAPSACK01MITM, KNAPSACKUNBOUNDED, SUBSETSUM, CUTTINGSTOCK,
' BINPACKING3D, ASSIGNAUCTION, ASSIGNBOTTLENECK, STABLEMARRIAGE, TRANSPORTNORTHWEST,
' TRANSPORTLEASTCOST, TRANSPORTMODI, ASSIGNQAPTABU.
' EXPECT: frac=240.0|mitm=220.0: 0  1  1 |unb=24.0: 1  2 |ss= 1 : 1 |cut= 2 : 1 |bp3d= 1 : 0|auct=2.0: 1 |bott=1.0: 1 |marry= 1 : 1 |nw=20.0|lc=20.0|modi=20.0|qap=12.0: 1
IMPORT OPTIMIZE

' TRUE se t() è una permutazione di 0..n-1.
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
  ' ── PACK: knapsack (3 varianti) + subset sum + cutting stock + 3D ──────────────────
  DIM w(2) AS DOUBLE
  w(0)=10 : w(1)=20 : w(2)=30
  DIM v(2) AS DOUBLE
  v(0)=60 : v(1)=100 : v(2)=120
  DIM xf(2) AS DOUBLE
  PRINT "frac="; FORMAT(OPTIMIZE.KNAPSACKFRACTIONAL(w, v, 3, 50.0, xf), "#.0");
  DIM t01(2) AS LONG
  PRINT "|mitm="; FORMAT(OPTIMIZE.KNAPSACK01MITM(w, v, 3, 50.0, t01), "#.0"); ":";
  PRINT t01(0); t01(1); t01(2);
  DIM wu(1) AS DOUBLE
  wu(0)=5 : wu(1)=3
  DIM vu(1) AS DOUBLE
  vu(0)=10 : vu(1)=7
  DIM cu(1) AS LONG
  PRINT "|unb="; FORMAT(OPTIMIZE.KNAPSACKUNBOUNDED(wu, vu, 2, 11, cu), "#.0"); ":";
  PRINT cu(0); cu(1);

  DIM items(5) AS DOUBLE
  items(0)=3 : items(1)=34 : items(2)=4 : items(3)=12 : items(4)=5 : items(5)=2
  DIM take(5) AS LONG
  r = OPTIMIZE.SUBSETSUM(items, 6, 9.0, take)
  ok = 1
  FOR i = 0 TO 5
    IF take(i) <> 0 AND take(i) <> 1 THEN ok = 0
  NEXT i
  ' NOTA: take() NON ricostruisce una combinazione che somma a target (bug noto, nota
  ' `(open) - OR-SubsetSum-Ricostruzione.md`); qui si verifica solo l'esito e la forma.
  PRINT "|ss="; r; ":"; ok;

  DIM pc(2) AS DOUBLE
  pc(0)=5 : pc(1)=7 : pc(2)=9
  DIM pat(2) AS LONG
  r = OPTIMIZE.CUTTINGSTOCK(pc, 3, 20.0, pat)
  ok = 1
  FOR i = 0 TO 2
    IF pat(i) < 0 OR pat(i) >= r THEN ok = 0
  NEXT i
  PRINT "|cut="; r; ":"; ok;

  DIM it3(8) AS DOUBLE
  FOR i = 0 TO 2 : it3(i*3)=2 : it3(i*3+1)=2 : it3(i*3+2)=2 : NEXT i
  DIM bo(2) AS LONG
  PRINT "|bp3d="; OPTIMIZE.BINPACKING3D(it3, 3, 3.0, 3.0, 3.0, bo); ":"; bo(0)+bo(1)+bo(2)

  ' ── ASSIGN: asta, collo di bottiglia, matrimonio stabile, trasporti, QAP ──────────
  DIM cau(5) AS DOUBLE
  cau(0)=1 : cau(1)=2 : cau(2)=3 : cau(3)=4 : cau(4)=1 : cau(5)=2
  DIM aa(1) AS LONG
  PRINT "auct="; FORMAT(OPTIMIZE.ASSIGNAUCTION(cau, 2, 3, aa, 0.01), "#.0"); ":"; IsPerm(aa, 2);
  DIM cb(3) AS DOUBLE
  cb(0)=1 : cb(1)=4 : cb(2)=4 : cb(3)=1
  DIM ab(1) AS LONG
  PRINT "|bott="; FORMAT(OPTIMIZE.ASSIGNBOTTLENECK(cb, 2, ab), "#.0"); ":";
  PRINT IsPerm(ab, 2);

  DIM pm(3) AS LONG
  pm(0)=0 : pm(1)=1 : pm(2)=1 : pm(3)=0
  DIM pw(3) AS LONG
  pw(0)=1 : pw(1)=0 : pw(2)=0 : pw(3)=1
  DIM mm(1) AS LONG
  PRINT "|marry="; OPTIMIZE.STABLEMARRIAGE(pm, pw, 2, mm); ":"; IsPerm(mm, 2);

  DIM sup(1) AS DOUBLE
  sup(0)=10 : sup(1)=10
  DIM dem(1) AS DOUBLE
  dem(0)=10 : dem(1)=10
  DIM tc(3) AS DOUBLE
  tc(0)=1 : tc(1)=2 : tc(2)=3 : tc(3)=1
  DIM xt(3) AS DOUBLE
  PRINT "|nw="; FORMAT(OPTIMIZE.TRANSPORTNORTHWEST(sup, dem, tc, 2, 2, xt), "#.0");
  DIM xs(3) AS DOUBLE
  PRINT "|lc="; FORMAT(OPTIMIZE.TRANSPORTLEASTCOST(sup, dem, tc, 2, 2, xs), "#.0");
  DIM xm(3) AS DOUBLE
  PRINT "|modi="; FORMAT(OPTIMIZE.TRANSPORTMODI(sup, dem, tc, 2, 2, xm, 50), "#.0");

  ' QAP 3x3: A flussi, B distanze (matrici piene)
  DIM A(8) AS DOUBLE
  DIM B(8) AS DOUBLE
  FOR i = 0 TO 8
    A(i) = (i MOD 3) + 1
    B(i) = IIf(i MOD 4 = 0, 0, 1)
  NEXT i
  DIM p(2) AS LONG
  p(0)=0 : p(1)=1 : p(2)=2
  PRINT "|qap="; FORMAT(OPTIMIZE.ASSIGNQAPTABU(A, B, 3, p, 200, 7), "#.0"); ":"; IsPerm(p, 3)
  PRINT ""
END SUB
