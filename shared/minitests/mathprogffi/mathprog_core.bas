' OPTIMIZE (FFI mathprog) — LP, MILP e model builder.
' EXPECT: lp= 0 :1.0|milp= 0 :-6.0|model= 0 :3.0: 2 :0.0:3.0|json=-1 
IMPORT OPTIMIZE

SUB Main()
  ' LP: min x+y  s.t. x+y=1, x,y>=0
  DIM A(1) AS DOUBLE
  A(0)=1 : A(1)=1
  DIM b(0) AS DOUBLE
  b(0)=1
  DIM c(1) AS DOUBLE
  c(0)=1 : c(1)=1
  DIM x(1) AS DOUBLE
  DIM y(0) AS DOUBLE
  DIM info(0) AS DOUBLE
  PRINT "lp="; OPTIMIZE.LPSOLVE(A, b, c, 1, 2, x, y, info); ":"; FORMAT(info(0), "#.0");

  ' MILP: min -2x-y  s.t. x+y=3, intere -> x=3,y=0, obj -6
  DIM c2(1) AS DOUBLE
  c2(0)=-2 : c2(1)=-1
  DIM b2(0) AS DOUBLE
  b2(0)=3
  DIM ii(2) AS LONG
  ii(0)=2 : ii(1)=0 : ii(2)=1
  DIM x2(1) AS DOUBLE
  DIM info2(0) AS DOUBLE
  PRINT "|milp="; OPTIMIZE.MILPBB(A, b2, c2, ii, x2, info2, 1, 2); ":"; FORMAT(info2(0), "#.0");

  ' Model builder: min 2x+y s.t. x+y=3, 0<=x,y<=10
  h = OPTIMIZE.MODELNEW()
  v0 = OPTIMIZE.MODELADDVAR(h, 0, 0.0, 10.0, "x")
  v1 = OPTIMIZE.MODELADDVAR(h, 0, 0.0, 10.0, "y")
  DIM mv(1) AS LONG
  mv(0)=v0 : mv(1)=v1
  DIM mc(1) AS DOUBLE
  mc(0)=1 : mc(1)=1
  r = OPTIMIZE.MODELADDCONSTR(h, mv, mc, 2, 2, 3.0)
  DIM oc(1) AS DOUBLE
  oc(0)=2 : oc(1)=1
  OPTIMIZE.MODELSETOBJ(h, mv, oc, 2, 0)
  DIM o(0) AS DOUBLE
  st = OPTIMIZE.MODELSOLVE(h, o)
  PRINT "|model="; st; ":"; FORMAT(o(0), "#.0");
  PRINT ":"; OPTIMIZE.MODELNUMVARS(h); ":"; FORMAT(OPTIMIZE.MODELVARVALUE(h, 0), "#.0");
  PRINT ":"; FORMAT(OPTIMIZE.MODELVARVALUE(h, 1), "#.0");
  js$ = OPTIMIZE.MODELTOJSON(h)
  PRINT "|json="; (LEN(js$) > 0)
  OPTIMIZE.MODELFREE(h)
  PRINT ""
END SUB
