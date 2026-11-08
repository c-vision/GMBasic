' MiniTest NAMESPACE MATH — tutti i 42 simboli della superficie.
'
' Superficie (shared/spec/registry/surface.csv): ABS, ACOS, ANGLE2D, ASIN, ATN, AVG,
' COSH, CEILING, CLAMP, COMB, COS, DEG2RAD, DIST2D, DIST3D, EXP, FACT, FIX, FLOOR,
' HYPOT, INT, ISNUMERIC, LOG, LOG10, MAX, MIN, POLAR_X, POLAR_Y, PERM, PI,
' PYTHAGORAS, RAD2DEG, RANGE, RND, ROUND, SINH, SGN, SIN, SQR, SUM, TANH, TAN, TRUNC.
'
' Scopo: verificare che i 4 backend supportino la STESSA versione del namespace,
' non un'applicazione. I valori trascendenti sono arrotondati con ROUND.
' EXPECT: abs= 5 |sgn=-1 , 0 , 1|fact= 120 |comb= 10 |perm= 20|ceil= 3 |floor= 2 |fix=-2|int=-3 |trunc=-2 |round= 3|clamp= 3 |max= 7 |min= 3|sum= 6 |avg= 3|sumArr= 10 |avgArr= 2.5 |maxArr= 4 |minArr= 1|pi=-1|rangeN= 5 |r0= 1 |r4= 5|rnd=-1|sin= 0 |cos= 1 |tan= 0|asin= 1.570796 |acos= 0 |atn= 0.785398|sinh= 0 |cosh= 1 |tanh= 0|exp= 1 |log= 0 |log10= 2|sqr= 3 |hypot= 5 |pyth= 5|deg2rad= 3.1415926536 |rad2deg= 180|dist2d= 5 |dist3d= 5|angle2d= 0 |polarx= 2 |polary= 0 |angle90= 90|isnum=-1 | 0

SUB Main()
  ' ── aritmetica e combinatoria ────────────────────────────────────────────
  PRINT "abs="; ABS(-5); "|sgn="; SGN(-5); ","; SGN(0); ","; SGN(5)
  PRINT "fact="; FACT(5); "|comb="; COMB(5, 2); "|perm="; PERM(5, 2)

  ' ── arrotondamenti e limiti ──────────────────────────────────────────────
  PRINT "ceil="; CEILING(2.1); "|floor="; FLOOR(2.9); "|fix="; FIX(-2.7)
  PRINT "int="; INT(-2.7); "|trunc="; TRUNC(-2.7); "|round="; ROUND(2.6)
  PRINT "clamp="; CLAMP(5, 1, 3); "|max="; MAX(3, 7); "|min="; MIN(3, 7)

  ' ── aggregati ────────────────────────────────────────────────────────────
  PRINT "sum="; SUM(1, 2, 3); "|avg="; AVG(2, 4)
  DIM v(3) AS DOUBLE = {1, 2, 3, 4}
  PRINT "sumArr="; SUM(v); "|avgArr="; AVG(v); "|maxArr="; MAX(v); "|minArr="; MIN(v)

  ' ── costanti e sequenze ──────────────────────────────────────────────────
  PRINT "pi="; (PI() > 3.1415 AND PI() < 3.1416)
  DIM r = RANGE(1, 5)
  PRINT "rangeN="; LEN(r); "|r0="; r(0); "|r4="; r(4)
  PRINT "rnd="; (RND() >= 0 AND RND() < 1)

  ' ── trigonometria ────────────────────────────────────────────────────────
  PRINT "sin="; ROUND(SIN(0), 6); "|cos="; ROUND(COS(0), 6); "|tan="; ROUND(TAN(0), 6)
  PRINT "asin="; ROUND(ASIN(1), 6); "|acos="; ROUND(ACOS(1), 6); "|atn="; ROUND(ATN(1), 6)
  PRINT "sinh="; ROUND(SINH(0), 6); "|cosh="; ROUND(COSH(0), 6); "|tanh="; ROUND(TANH(0), 6)

  ' ── esponenziali e logaritmi ─────────────────────────────────────────────
  PRINT "exp="; ROUND(EXP(0), 6); "|log="; ROUND(LOG(1), 6); "|log10="; ROUND(LOG10(100), 6)
  PRINT "sqr="; ROUND(SQR(9), 6); "|hypot="; ROUND(HYPOT(3, 4), 6); "|pyth="; ROUND(PYTHAGORAS(3, 4), 6)

  ' ── angoli e geometria ───────────────────────────────────────────────────
  PRINT "deg2rad="; ROUND(DEG2RAD(180), 10); "|rad2deg="; ROUND(RAD2DEG(PI()), 6)
  ' DIST2D/DIST3D sono passate al namespace VECTOR (tassonomia math/stats/timeseries):
  ' unqualified non sono più risolte; la forma qualificata è quella canonica.
  PRINT "dist2d="; ROUND(VECTOR.DIST2D(0, 0, 3, 4), 6); "|dist3d="; ROUND(VECTOR.DIST3D(0, 0, 0, 0, 0, 5), 6)
  PRINT "angle2d="; ROUND(ANGLE2D(0, 0, 1, 0), 6); "|polarx="; ROUND(POLAR_X(2, 0), 6); "|polary="; ROUND(POLAR_Y(2, 0), 6); "|angle90="; ROUND(ANGLE2D(0, 0, 0, 1), 6)

  ' ── predicato numerico ───────────────────────────────────────────────────
  PRINT "isnum="; ISNUMERIC("12.5"); "|"; ISNUMERIC("abc")
END SUB
