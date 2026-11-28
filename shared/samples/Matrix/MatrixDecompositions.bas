' FILE: samples/Matrix/MatrixDecompositions.bas
' Sample COMPLETO delle decomposizioni via FFI (LINALG su array piatti) + operazioni MATRIX.
' A = [[4,2],[2,3]] simmetrica definita positiva; I = identità.
' Copre: CHOLESKY/SVD/EVD/LU/QR/EIG/FACTOR, INVERSE/PINVERSE/TRANSPOSE/MULTIPLY,
' TRACE/DETERMINANT/RANK/COND/NORM, EXPM/LOGM/SQRTM/COSM/SINM/TANM/SIGNM/
' FRACTIONALPOWER, HESSENBERG/SCHUR/POLAR/ORTH, BLOCKDIAG/CIRCULANT/HANKEL/
' KRONECKER/TOEPLITZ, NUCLEARNORM/NULLSPACE/SUBSPACEANGLES, SOLVE/SOLVETRIANGULAR.
IMPORT LINALG

SUB Main()
    ' A piatta row-major 2×2
    DIM fa(3) AS DOUBLE
    fa(0)=4 : fa(1)=2 : fa(2)=2 : fa(3)=3

    ' A come valore MATRIX (per le operazioni MATRIX.*)
    DIM a = MATRIX.CREATE(2, 2)
    a = MATRIX.SETVALUE(a, 0, 0, 4) : a = MATRIX.SETVALUE(a, 0, 1, 2)
    a = MATRIX.SETVALUE(a, 1, 0, 2) : a = MATRIX.SETVALUE(a, 1, 1, 3)

    PRINT "det="; MATH.ROUND(LINALG.DETERMINANT(fa, 2), 6)
    PRINT "trace="; MATH.ROUND(MATRIX.TRACE(a), 6)
    PRINT "rank="; LINALG.RANK(fa, 2, 2)
    PRINT "norm="; MATH.ROUND(LINALG.NORM(fa, 2, 2), 6)
    PRINT "cond="; MATH.ROUND(LINALG.COND(fa, 2, 2), 4)

    DIM inv(3) AS DOUBLE
    LINALG.INVERSE(fa, 2, inv)
    PRINT "inv00="; MATH.ROUND(inv(0), 6)
    DIM pinv(3) AS DOUBLE
    LINALG.PINVERSE(fa, 2, 2, pinv)
    PRINT "pinv00="; MATH.ROUND(pinv(0), 6)
    DIM tr = MATRIX.TRANSPOSE(a)
    PRINT "tr01="; MATH.ROUND(MATRIX.GETVALUE(tr, 0, 1), 6)
    DIM mul = MATRIX.MULTIPLY(a, a)
    PRINT "mul00="; MATH.ROUND(MATRIX.GETVALUE(mul, 0, 0), 6)

    ' decomposizioni + FACTOR (accessore FFI)
    DIM out(3) AS DOUBLE, meta(1) AS LONG
    LINALG.FACTOR(fa, 2, 2, "CHOL.L", out, meta)
    PRINT "chol_l00="; MATH.ROUND(out(0), 6)
    LINALG.FACTOR(fa, 2, 2, "SVD.S", out, meta)
    PRINT "svd_s00="; MATH.ROUND(out(0), 6)
    LINALG.FACTOR(fa, 2, 2, "EVD.D", out, meta)
    PRINT "evd_d00="; MATH.ROUND(out(0), 6)
    LINALG.FACTOR(fa, 2, 2, "LU.U", out, meta)
    PRINT "lu_u00="; MATH.ROUND(out(0), 6)
    LINALG.FACTOR(fa, 2, 2, "QR.Q", out, meta)
    PRINT "qr_q00="; MATH.ROUND(out(0), 6)
    LINALG.FACTOR(fa, 2, 2, "EIG.DREAL", out, meta)
    PRINT "eig="; MATH.ROUND(out(0), 4)

    ' funzioni matriciali
    DIM mf(3) AS DOUBLE
    LINALG.EXPM(fa, 2, mf)
    PRINT "expm00="; MATH.ROUND(mf(0), 4)
    LINALG.LOGM(fa, 2, mf)
    PRINT "logm00="; MATH.ROUND(mf(0), 4)
    LINALG.SQRTM(fa, 2, mf)
    PRINT "sqrtm00="; MATH.ROUND(mf(0), 4)
    LINALG.COSM(fa, 2, mf)
    PRINT "cosm00="; MATH.ROUND(mf(0), 4)
    LINALG.SINM(fa, 2, mf)
    PRINT "sinm00="; MATH.ROUND(mf(0), 4)
    LINALG.TANM(fa, 2, mf)
    PRINT "tanm00="; MATH.ROUND(mf(0), 4)
    LINALG.SIGNM(fa, 2, mf)
    PRINT "signm00="; MATH.ROUND(mf(0), 4)
    LINALG.FRACTIONALPOWER(fa, 2, 0.5, mf)
    PRINT "fracpow00="; MATH.ROUND(mf(0), 4)

    ' fattorizzazioni avanzate
    DIM hh(3) AS DOUBLE
    LINALG.HESSENBERG(fa, 2, hh)
    PRINT "hess00="; MATH.ROUND(hh(0), 4)
    LINALG.FACTOR(fa, 2, 2, "SCHUR.T", out, meta)
    PRINT "schur00="; MATH.ROUND(out(0), 4)
    LINALG.FACTOR(fa, 2, 2, "POLAR.U", out, meta)
    PRINT "polar00="; MATH.ROUND(out(0), 4)
    DIM o(3) AS DOUBLE
    LINALG.ORTH(fa, 2, 2, o)
    PRINT "orth00="; MATH.ROUND(o(0), 4)

    ' costruzione matrici (CIRCULANT/HANKEL/TOEPLITZ prendono array grezzi)
    PRINT "blockdiag00="; MATH.ROUND(MATRIX.GETVALUE(MATRIX.BLOCKDIAG(a, a), 0, 0), 4)
    DIM cc(2)
    cc(0) = 1 : cc(1) = 2 : cc(2) = 3
    DIM cv = MATRIX.CIRCULANT(cc)
    PRINT "circulant00="; MATH.ROUND(MATRIX.GETVALUE(cv, 0, 0), 4)
    DIM hc(2), hr(2)
    hc(0) = 1 : hc(1) = 2 : hc(2) = 3
    hr(0) = 4 : hr(1) = 5 : hr(2) = 6
    DIM hk = MATRIX.HANKEL(hc, hr)
    PRINT "hankel00="; MATH.ROUND(MATRIX.GETVALUE(hk, 0, 0), 4)
    DIM toc(2), tor(2)
    toc(0) = 1 : toc(1) = 2 : toc(2) = 3
    tor(0) = 4 : tor(1) = 5 : tor(2) = 6
    DIM tp = MATRIX.TOEPLITZ(toc, tor)
    PRINT "toeplitz00="; MATH.ROUND(MATRIX.GETVALUE(tp, 0, 0), 4)
    DIM kr = MATRIX.KRONECKER(a, a)
    PRINT "kron00="; MATH.ROUND(MATRIX.GETVALUE(kr, 0, 0), 4)

    ' norme / spazi nulli
    PRINT "nuclearnorm="; MATH.ROUND(LINALG.NUCLEARNORM(fa, 2, 2), 4)
    ' NULLSPACE su matrice singolare
    DIM sg(3) AS DOUBLE
    sg(0) = 1 : sg(1) = 2
    sg(2) = 2 : sg(3) = 4
    DIM ns(1) AS DOUBLE
    LINALG.NULLSPACE(sg, 2, 2, ns)
    PRINT "nullspace00="; MATH.ROUND(ns(0), 4)
    ' sottospazi: col(A) e col(A·A) coincidono -> angoli 0
    DIM mulflat(3) AS DOUBLE
    mulflat(0)=20 : mulflat(1)=14 : mulflat(2)=14 : mulflat(3)=10
    DIM sa(1) AS DOUBLE
    DIM nsa AS LONG
    nsa = LINALG.SUBSPACEANGLES(fa, 2, 2, mulflat, 2, 2, sa)
    PRINT "subspace00="; MATH.ROUND(sa(0), 4)

    ' risoluzione sistemi
    DIM b(1) AS DOUBLE
    b(0) = 8 : b(1) = 7
    DIM xs(1) AS DOUBLE
    LINALG.SOLVE(fa, 2, b, xs)
    PRINT "solve0="; MATH.ROUND(xs(0), 4)
    DIM at(1) AS DOUBLE
    at(0) = 4 : at(1) = 3
    DIM xt(1) AS DOUBLE
    LINALG.SOLVETRIANGULAR(fa, 2, at, 0, xt)
    PRINT "solvetri0="; MATH.ROUND(xt(0), 4)

    PRINT "Sample MATRIX decomposizioni completato."
END SUB
