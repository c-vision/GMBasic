' NEURAL (FFI) — autodiff scalare (micrograd) e programmazione genetica (gn_gp).
' EXPECT: z= 7004 :8.0|b= 0 |g=4.0,2.0,1.0|w=9.0:6.0,-6.0|t=0.0000|f= 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 |dv=2.0,1.0000,-3.0,8.0,0.0|gp= 7101 : 9 :26.0: 3 , 3 , 3 , 3 , 3 , 3 , 3 , 3 , 2 ,|ev=6.0|gpf= 0
IMPORT NEURAL

' Fitness del GP: somma dei codici del genoma (da massimizzare).
FUNCTION GpFit(code() AS DOUBLE) AS DOUBLE
  DIM i AS LONG, s AS DOUBLE
  s = 0
  FOR i = 0 TO UBOUND(code)
    s = s + code(i)
  NEXT i
  RETURN s
END FUNCTION

SUB Main()
  ' z = x*y + x con x=2, y=3 → dz/dx = y+1 = 4, dz/dy = x = 2
  DIM X AS LONG, Y AS LONG, M AS LONG, Z AS LONG
  X = NEURAL.AUTOVALUE(2)
  Y = NEURAL.AUTOVALUE(3)
  M = NEURAL.AUTOMUL(X, Y)
  Z = NEURAL.AUTOADD(M, X)
  PRINT "z="; Z; ":"; FORMAT(NEURAL.AUTODATA(Z), "#.0"); "|b="; NEURAL.AUTOBACKWARD(Z); "|";
  PRINT "g="; FORMAT(NEURAL.AUTOGRAD(X), "#.0"); ","; FORMAT(NEURAL.AUTOGRAD(Y), "#.0");
  PRINT ","; FORMAT(NEURAL.AUTOGRAD(M), "#.0"); "|";

  ' w = (x - y)^2 con x=5, y=2 → w = 9, dw/dx = 6, dw/dy = -6
  DIM X2 AS LONG, Y2 AS LONG, D AS LONG, W AS LONG
  X2 = NEURAL.AUTOVALUE(5)
  Y2 = NEURAL.AUTOVALUE(2)
  D = NEURAL.AUTOSUB(X2, Y2)
  W = NEURAL.AUTOMUL(D, D)
  NEURAL.AUTOBACKWARD(W)
  PRINT "w="; FORMAT(NEURAL.AUTODATA(W), "#.0"); ":"; FORMAT(NEURAL.AUTOGRAD(X2), "#.0"); ","; FORMAT(NEURAL.AUTOGRAD(Y2), "#.0"); "|";
  PRINT "t="; FORMAT(NEURAL.AUTODATA(NEURAL.AUTOTANH(NEURAL.AUTOVALUE(0))), "#.0000"); "|";
  PRINT "f="; NEURAL.AUTOFREE(Z); ","; NEURAL.AUTOFREE(M); ","; NEURAL.AUTOFREE(X); ","; NEURAL.AUTOFREE(Y);
  PRINT ","; NEURAL.AUTOFREE(W); ","; NEURAL.AUTOFREE(D); ","; NEURAL.AUTOFREE(X2); ","; NEURAL.AUTOFREE(Y2); "|";

  ' ── operazioni mancanti: DIV, EXP, NEG, POW, RELU ──
  DIM U AS LONG, V AS LONG, Q AS LONG, T AS LONG
  U = NEURAL.AUTOVALUE(2) : V = NEURAL.AUTOVALUE(4)
  Q = NEURAL.AUTODIV(V, U)
  PRINT "dv="; FORMAT(NEURAL.AUTODATA(Q), "#.0");
  NEURAL.AUTOFREE(Q)
  T = NEURAL.AUTOVALUE(0)
  Q = NEURAL.AUTOEXP(T)
  PRINT ","; FORMAT(NEURAL.AUTODATA(Q), "#.0000");
  NEURAL.AUTOFREE(Q) : NEURAL.AUTOFREE(T)
  T = NEURAL.AUTOVALUE(3)
  Q = NEURAL.AUTONEG(T)
  PRINT ","; FORMAT(NEURAL.AUTODATA(Q), "#.0");
  NEURAL.AUTOFREE(Q) : NEURAL.AUTOFREE(T)
  Q = NEURAL.AUTOPOW(U, 3)
  PRINT ","; FORMAT(NEURAL.AUTODATA(Q), "#.0");
  NEURAL.AUTOFREE(Q)
  T = NEURAL.AUTOVALUE(-1)
  Q = NEURAL.AUTORELU(T)
  PRINT ","; FORMAT(NEURAL.AUTODATA(Q), "#.0"); "|";
  NEURAL.AUTOFREE(Q) : NEURAL.AUTOFREE(T)
  NEURAL.AUTOFREE(U) : NEURAL.AUTOFREE(V)

  ' GP: alfabeto {1,2,3} con arita' {0,0,1}; massimizza la somma dei codici.
  DIM codes(2) AS LONG, arity(2) AS LONG, best(62) AS LONG, gfit(0) AS DOUBLE
  codes(0)=1 : codes(1)=2 : codes(2)=3
  arity(0)=0 : arity(1)=0 : arity(2)=1
  gh = NEURAL.GPINIT(codes, arity, 3)
  PRINT "gp="; gh; ":";
  DIM n AS LONG
  n = NEURAL.GPEVOLVE(gh, GpFit, best, gfit, 6, 5, 42, 2)
  PRINT n; ":"; FORMAT(gfit(0), "#.0"); ":";
  DIM i AS LONG
  FOR i = 0 TO n - 1
    PRINT best(i); ",";
  NEXT i
  PRINT "|ev="; FORMAT(NEURAL.GPEVAL(gh, codes, 3, GpFit), "#.0");
  PRINT "|gpf="; NEURAL.GPFREE(gh)
END SUB
