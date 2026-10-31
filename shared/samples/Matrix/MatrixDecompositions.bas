' FILE: samples/Community/Matrix/MatrixDecompositions.bas
' Sample COMPLETO delle funzioni MATRIX (decomposizioni + operazioni matriciali).
' A = [[4,2],[2,3]] simmetrica definita positiva; I = identità.
' Copre: CHOLESKY/SVD/EVD/LU/QR/EIG/FACTOR, INVERSE/PINVERSE/TRANSPOSE/MULTIPLY,
' TRACE/DETERMINANT/RANK/COND/NORM, EXPM/LOGM/SQRTM/COSM/SINM/TANM/SIGNM/
' FRACTIONALPOWER, HESSENBERG/SCHUR/POLAR/ORTH, BLOCKDIAG/CIRCULANT/HANKEL/
' KRONECKER/TOEPLITZ, NUCLEARNORM/NULLSPACE/SUBSPACEANGLES, SOLVE/SOLVETRIANGULAR.
IMPORT MATRIX
IMPORT MATH

SUB Main()
    DIM a = MATRIX.CREATE(2, 2)
    a = MATRIX.SETVALUE(a, 0, 0, 4) : a = MATRIX.SETVALUE(a, 0, 1, 2)
    a = MATRIX.SETVALUE(a, 1, 0, 2) : a = MATRIX.SETVALUE(a, 1, 1, 3)

    PRINT "det="; MATH.ROUND(MATRIX.DETERMINANT(a), 6)
    PRINT "trace="; MATH.ROUND(MATRIX.TRACE(a), 6)
    PRINT "rank="; MATRIX.RANK(a)
    PRINT "norm="; MATH.ROUND(MATRIX.NORM(a), 6)
    PRINT "cond="; MATH.ROUND(MATRIX.COND(a), 4)

    DIM inv = MATRIX.INVERSE(a)
    PRINT "inv00="; MATH.ROUND(MATRIX.GETVALUE(inv, 0, 0), 6)
    DIM pinv = MATRIX.PINVERSE(a)
    PRINT "pinv00="; MATH.ROUND(MATRIX.GETVALUE(pinv, 0, 0), 6)
    DIM tr = MATRIX.TRANSPOSE(a)
    PRINT "tr01="; MATH.ROUND(MATRIX.GETVALUE(tr, 0, 1), 6)
    DIM mul = MATRIX.MULTIPLY(a, a)
    PRINT "mul00="; MATH.ROUND(MATRIX.GETVALUE(mul, 0, 0), 6)

    ' decomposizioni + FACTOR
    DIM ch = MATRIX.CHOLESKY(a)
    DIM cl = MATRIX.FACTOR(ch, "L")
    PRINT "chol_l00="; MATH.ROUND(MATRIX.GETVALUE(cl, 0, 0), 6)
    DIM svd = MATRIX.SVD(a)
    DIM ss = MATRIX.FACTOR(svd, "S")
    PRINT "svd_s00="; MATH.ROUND(MATRIX.GETVALUE(ss, 0, 0), 6)
    DIM evd = MATRIX.EVD(a)
    DIM ed = MATRIX.FACTOR(evd, "D")
    PRINT "evd_d00="; MATH.ROUND(MATRIX.GETVALUE(ed, 0, 0), 6)
    DIM lu = MATRIX.LU(a)
    DIM luu = MATRIX.FACTOR(lu, "U")
    PRINT "lu_u00="; MATH.ROUND(MATRIX.GETVALUE(luu, 0, 0), 6)
    DIM qr = MATRIX.QR(a)
    DIM qrq = MATRIX.FACTOR(qr, "Q")
    PRINT "qr_q00="; MATH.ROUND(MATRIX.GETVALUE(qrq, 0, 0), 6)
    DIM eig = MATRIX.EIG(a)
    PRINT "eig="; eig

    ' funzioni matriciali
    PRINT "expm00="; MATH.ROUND(MATRIX.GETVALUE(MATRIX.EXPM(a), 0, 0), 4)
    PRINT "logm00="; MATH.ROUND(MATRIX.GETVALUE(MATRIX.LOGM(a), 0, 0), 4)
    PRINT "sqrtm00="; MATH.ROUND(MATRIX.GETVALUE(MATRIX.SQRTM(a), 0, 0), 4)
    PRINT "cosm00="; MATH.ROUND(MATRIX.GETVALUE(MATRIX.COSM(a), 0, 0), 4)
    PRINT "sinm00="; MATH.ROUND(MATRIX.GETVALUE(MATRIX.SINM(a), 0, 0), 4)
    PRINT "tanm00="; MATH.ROUND(MATRIX.GETVALUE(MATRIX.TANM(a), 0, 0), 4)
    PRINT "signm00="; MATH.ROUND(MATRIX.GETVALUE(MATRIX.SIGNM(a), 0, 0), 4)
    PRINT "fracpow00="; MATH.ROUND(MATRIX.GETVALUE(MATRIX.FRACTIONALPOWER(a, 0.5), 0, 0), 4)

    ' fattorizzazioni avanzate
    PRINT "hess00="; MATH.ROUND(MATRIX.GETVALUE(MATRIX.HESSENBERG(a), 0, 0), 4)
    PRINT "schur00="; MATH.ROUND(MATRIX.GETVALUE(MATRIX.SCHUR(a), 0, 0), 4)
    PRINT "polar00="; MATH.ROUND(MATRIX.GETVALUE(MATRIX.POLAR(a), 0, 0), 4)
    PRINT "orth00="; MATH.ROUND(MATRIX.GETVALUE(MATRIX.ORTH(a), 0, 0), 4)

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
    PRINT "nuclearnorm="; MATH.ROUND(MATRIX.NUCLEARNORM(a), 4)
    DIM ns = MATRIX.NULLSPACE(a)
    PRINT "nullspace00="; MATH.ROUND(MATRIX.GETVALUE(ns, 0, 0), 4)
    DIM sa = MATRIX.SUBSPACEANGLES(a, mul)
    PRINT "subspace00="; MATH.ROUND(sa(0), 4)

    ' risoluzione sistemi
    DIM b(1)
    b(0) = 8 : b(1) = 7
    DIM xs() = MATRIX.SOLVE(a, b)
    PRINT "solve0="; MATH.ROUND(xs(0), 4)
    DIM at(1)
    at(0) = 4 : at(1) = 3
    DIM xt() = MATRIX.SOLVETRIANGULAR(a, at, 0)
    PRINT "solvetri0="; MATH.ROUND(xt(0), 4)

    PRINT "Sample MATRIX decomposizioni completato."
END SUB
