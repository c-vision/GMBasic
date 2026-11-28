' LINALG — campione completo della superficie FFI (algebra lineare su array piatti).
'
' Mostra in un unico flusso realistico tutte le funzioni del namespace LINALG:
' scalari/norme, sistemi lineari e triangolari, decomposizioni (Cholesky, LU, QR,
' SVD, Hessenberg, Schur, pinv, orth, nullspace), autovalori/autovettori (EVD/EIG),
' funzioni matriciali (exp/log/sqrt/sin/cos/tan/sign/potenza frazionaria/polar) e
' angoli fra sottospazi. Le matrici sono array 1-D row-major: il contratto ABI del
' namespace (una sola lunghezza per parametro array).
'
' Verifica: zsh tools/sh/parity4 ../shared/samples/Math/LinalgFFISurface.bas
IMPORT LINALG

SUB Main()
  PRINT "== scalari e norme"
  ' A = [[4,2],[2,3]] SPD
  DIM a(3) AS DOUBLE
  a(0)=4 : a(1)=2 : a(2)=2 : a(3)=3
  PRINT "det="; ROUND(LINALG.DETERMINANT(a, 2), 6)
  PRINT "rank="; LINALG.RANK(a, 2, 2)
  PRINT "norm="; ROUND(LINALG.NORM(a, 2, 2), 6)
  PRINT "cond="; ROUND(LINALG.COND(a, 2, 2), 6)
  PRINT "nuclear="; ROUND(LINALG.NUCLEARNORM(a, 2, 2), 6)
  PRINT "fro="; ROUND(LINALG.FROBENIUSNORM(a, 2, 2), 6)
  PRINT "one="; ROUND(LINALG.ONENORM(a, 2, 2), 6)
  PRINT "inf="; ROUND(LINALG.INFINITYNORM(a, 2, 2), 6)

  PRINT "== sistemi lineari"
  DIM b(1) AS DOUBLE
  b(0)=6 : b(1)=5
  DIM x(1) AS DOUBLE
  PRINT "solve="; LINALG.SOLVE(a, 2, b, x); " x0="; ROUND(x(0), 6); " x1="; ROUND(x(1), 6)
  DIM inv(3) AS DOUBLE
  PRINT "inv="; LINALG.INVERSE(a, 2, inv); " i00="; ROUND(inv(0), 6)
  ' triangolare inferiore L = [[4,0],[2,2]] (prima colonna della Cholesky sotto)
  DIM lt(3) AS DOUBLE
  lt(0)=4 : lt(1)=0 : lt(2)=2 : lt(3)=2
  DIM xt(1) AS DOUBLE
  PRINT "tri="; LINALG.SOLVETRIANGULAR(lt, 2, b, 1, xt); " t0="; ROUND(xt(0), 6)

  PRINT "== decomposizioni"
  DIM chl(3) AS DOUBLE
  PRINT "chol="; LINALG.CHOLESKY(a, 2, chl); " L11="; ROUND(chl(3), 6)
  DIM lu_l(3) AS DOUBLE, lu_u(3) AS DOUBLE, lu_p(3) AS DOUBLE
  PRINT "lu="; LINALG.LU(a, 2, lu_l, lu_u, lu_p); " U00="; ROUND(lu_u(0), 6)
  DIM q(3) AS DOUBLE, r(3) AS DOUBLE
  PRINT "qr="; LINALG.QR(a, 2, 2, q, r); " R00="; ROUND(r(0), 6)
  DIM su(3) AS DOUBLE, ss(1) AS DOUBLE, svt(3) AS DOUBLE
  PRINT "svd="; LINALG.SVD(a, 2, 2, su, ss, svt); " s0="; ROUND(ss(0), 6); " s1="; ROUND(ss(1), 6)
  DIM hh(3) AS DOUBLE
  PRINT "hess="; LINALG.HESSENBERG(a, 2, hh); " h10="; ROUND(hh(2), 6)
  DIM t(3) AS DOUBLE, z(3) AS DOUBLE
  PRINT "schur="; LINALG.SCHUR(a, 2, t, z); " t00="; ROUND(t(0), 6)
  DIM pin(3) AS DOUBLE
  PRINT "pinv="; LINALG.PINVERSE(a, 2, 2, pin); " p00="; ROUND(pin(0), 6)
  ' B = [[1,0],[1,1],[0,1]] (3×2), spazio colonne 2-D
  DIM bb(5) AS DOUBLE
  bb(0)=1 : bb(1)=0
  bb(2)=1 : bb(3)=1
  bb(4)=0 : bb(5)=1
  DIM ob(5) AS DOUBLE
  PRINT "orth="; LINALG.ORTH(bb, 3, 2, ob)
  ' C = [[1,2],[2,4]] rango 1: nucleo = [2,-1]/sqrt(5)
  DIM cc(3) AS DOUBLE
  cc(0)=1 : cc(1)=2 : cc(2)=2 : cc(3)=4
  DIM ns(1) AS DOUBLE
  PRINT "null="; LINALG.NULLSPACE(cc, 2, 2, ns); " n0="; ROUND(ABS(ns(0)), 6)

  PRINT "== autovalori e autovettori"
  DIM d(1) AS DOUBLE, V(3) AS DOUBLE
  PRINT "evd="; LINALG.EVD(a, 2, d, V); " d0="; ROUND(d(0), 6); " d1="; ROUND(d(1), 6)
  ' rotazione [[0,-1],[1,0]]: autovalori ±i
  DIM rot(3) AS DOUBLE
  rot(0)=0 : rot(1)=-1 : rot(2)=1 : rot(3)=0
  DIM wr(1) AS DOUBLE, wi(1) AS DOUBLE, vr(3) AS DOUBLE, vi(3) AS DOUBLE
  PRINT "eig="; LINALG.EIG(rot, 2, wr, wi, vr, vi); " wi="; ROUND(ABS(wi(0)), 6)

  PRINT "== funzioni matriciali"
  DIM ex(3) AS DOUBLE
  PRINT "expm="; LINALG.EXPM(a, 2, ex); " e00="; ROUND(ex(0), 6)
  DIM lg(3) AS DOUBLE
  PRINT "logm="; LINALG.LOGM(a, 2, lg); " l00="; ROUND(lg(0), 6)
  DIM sq(3) AS DOUBLE
  PRINT "sqrtm="; LINALG.SQRTM(a, 2, sq); " s00="; ROUND(sq(0), 6)
  DIM cs(3) AS DOUBLE
  PRINT "cosm="; LINALG.COSM(a, 2, cs); " c00="; ROUND(cs(0), 6)
  DIM sn(3) AS DOUBLE
  PRINT "sinm="; LINALG.SINM(a, 2, sn); " s00="; ROUND(sn(0), 6)
  DIM tn(3) AS DOUBLE
  PRINT "tanm="; LINALG.TANM(a, 2, tn); " t00="; ROUND(ABS(tn(0)), 6)
  DIM sg(3) AS DOUBLE
  PRINT "signm="; LINALG.SIGNM(a, 2, sg); " s00="; ROUND(sg(0), 6)
  DIM fp(3) AS DOUBLE
  PRINT "frac="; LINALG.FRACTIONALPOWER(a, 2, 0.5, fp); " f00="; ROUND(fp(0), 6)
  DIM pu(3) AS DOUBLE, pp(3) AS DOUBLE
  PRINT "polar="; LINALG.POLAR(a, 2, pu, pp); " p00="; ROUND(pp(0), 6)

  PRINT "== sottospazi"
  ' col(A1) = (1,0), col(A2) = (1,1) -> 45 gradi
  DIM u1(1) AS DOUBLE
  u1(0)=1 : u1(1)=0
  DIM u2(1) AS DOUBLE
  u2(0)=1 : u2(1)=1
  DIM ang(0) AS DOUBLE
  PRINT "sub="; LINALG.SUBSPACEANGLES(u1, 2, 1, u2, 2, 1, ang); " ang0="; ROUND(ang(0), 6)

  PRINT "== delta: spazi, minimi quadrati, tridiagonali, iterativi"
  PRINT "trace="; ROUND(LINALG.TRACE(a, 2), 6)

  ' RREF di [[1,2],[2,4]] (rango 1)
  DIM qrr(3) AS DOUBLE
  qrr(0)=1 : qrr(1)=2 : qrr(2)=2 : qrr(3)=4
  DIM qro(3) AS DOUBLE
  PRINT "rref="; LINALG.RREF(qrr, 2, 2, qro); " o01="; ROUND(qro(1), 6)

  ' minimi quadrati [[1,0],[0,1],[1,1]] x = [1,2,3] -> [1,2]
  DIM qa(5) AS DOUBLE
  qa(0)=1 : qa(1)=0
  qa(2)=0 : qa(3)=1
  qa(4)=1 : qa(5)=1
  DIM qb(2) AS DOUBLE
  qb(0)=1 : qb(1)=2 : qb(2)=3
  DIM qx(1) AS DOUBLE
  PRINT "lstsq="; LINALG.LSTSQ(qa, 3, 2, qb, qx); " x0="; ROUND(qx(0), 6); " x1="; ROUND(qx(1), 6)

  ' tridiagonale [[2,1,0],[1,2,1],[0,1,2]] x = [3,4,3]
  DIM ql(2) AS DOUBLE, qd(2) AS DOUBLE, qu(2) AS DOUBLE, qrhs(2) AS DOUBLE
  ql(0)=1 : ql(1)=1 : ql(2)=0
  qd(0)=2 : qd(1)=2 : qd(2)=2
  qu(0)=1 : qu(1)=1 : qu(2)=0
  qrhs(0)=3 : qrhs(1)=4 : qrhs(2)=3
  DIM qtx(2) AS DOUBLE
  PRINT "thomas="; LINALG.THOMAS(ql, qd, qu, qrhs, 3, qtx); " x1="; ROUND(qtx(1), 6)

  ' LDLᵀ di [[4,2],[2,3]]
  DIM qs(3) AS DOUBLE
  qs(0)=4 : qs(1)=2 : qs(2)=2 : qs(3)=3
  DIM qll(3) AS DOUBLE, qld(1) AS DOUBLE
  PRINT "ldlt="; LINALG.LDLT(qs, 2, qll, qld); " d1="; ROUND(qld(1), 6)

  ' CG su [[4,1],[1,3]] b=[6,5]
  DIM qcb(1) AS DOUBLE, qcx(1) AS DOUBLE
  qcb(0)=6 : qcb(1)=5
  PRINT "cg="; LINALG.CG(a, 2, qcb, qcx, 50, 1e-12); " x0="; ROUND(qcx(0), 6)

  ' autovalore dominante di diag(2,1)
  DIM qp(3) AS DOUBLE
  qp(0)=2 : qp(1)=0 : qp(2)=0 : qp(3)=1
  DIM qlam(0) AS DOUBLE, qv(1) AS DOUBLE
  PRINT "power="; LINALG.POWER(qp, 2, 200, 1e-10, qlam, qv); " lam="; ROUND(qlam(0), 6)
END SUB
