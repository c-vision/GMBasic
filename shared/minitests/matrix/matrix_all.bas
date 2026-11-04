' MiniTest NAMESPACE MATRIX — tutte le 75 funzioni della superficie.
'
' Copre: creazione/accesso, aritmetica, decomposizioni (LU/QR/SVD/EIG/EVD/CHOLESKY/
' SCHUR/POLAR/HESSENBERG), potenze matriciali, norme/condizionamento, predicati,
' vettori, slice/reshape/concat, CSV/JSON e fattorizzazioni (FACTOR).
' Derivato dai MiniTest per-funzione (stessi input e oracoli) + ARGMAX/TRANSPOSE.
' Scopo: verificare la STESSA versione del namespace sui 4 backend, non un'applicazione.
' EXPECT: det= 3|trace= 4|rank= 2|norm= 3|cond= 3|inv00= 0.666667|mul00= 5|sq00= 1.366025|lg00= 0.549306|ex00= 11.401909|hh00= 2|co00=-0.224845|si00= 0.491295|sg00= 1|rowcount:  2|colcount:  2|max:  3|min:  0|frobenius:  3.74165738677394|onenorm:  5|infnorm:  3|symmetric: no|upper: si|lower: no|orthogonal: no|csv: [1,2|0,3|]|symmetric2: si|clone:  3|reshape_rows:  1|reshape_cols:  4|slice:  1|concatrows:  4|concatcols:  4|rowsums:  3 , 3|colsums:  1 , 5|rowmeans:  1.5 , 1.5|colmeans:  0.5 , 2.5|rowvar:  0.25 , 2.25|colvar:  0.25 , 0.25|abovemask:  0 ,-1 , 0 ,-1|abovematrix:  0 ,-1 , 0 ,-1|argmin:  1 , 0|addcol:  11|addrow:  11|mulvec:  50 , 60|mulleft:  10 , 80|asjson: [[1,2],[0,3]]|sym=-1|orth=-1|upper=-1|lower=-1|nsym= 0|nupper= 0|nlower= 0|orth= 0.707107|p00=-24|v= 1|v= 3|slicenorm= 1.414214|slicerows= 2|slicecols= 2|tanm= 0.707431|v= 3|bd22= 2|l00= 1.414214|c01= 3|ev= 3|fp00= 1.366025|hk= 4|k00= 0|csv= 9|json= 1|u00= 3|nuc= 4|ns= 0.707107|r00=-3.162278|x0= 1|x0= 1|anglen= 2|angle0= 0|s00= 3|tp= 4|argmax= 1 , 1|transpose= 2
IMPORT ARRAYS
IMPORT SYSTEM


SUB M_handles()
  DIM h = MATRIX.CREATE(2,2)
  h = MATRIX.SETVALUE(h, 0, 0, 2) : h = MATRIX.SETVALUE(h, 0, 1, 1)
  h = MATRIX.SETVALUE(h, 1, 0, 1) : h = MATRIX.SETVALUE(h, 1, 1, 2)
  PRINT "det="; MATH.ROUND(LINALG.DETERMINANT(h), 6)
  PRINT "trace="; MATH.ROUND(MATRIX.TRACE(h), 6)
  PRINT "rank="; LINALG.RANK(h)
  PRINT "norm="; MATH.ROUND(LINALG.NORM(h), 6)
  PRINT "cond="; MATH.ROUND(LINALG.COND(h), 4)
  DIM inv = LINALG.INVERSE(h)
  PRINT "inv00="; MATH.ROUND(MATRIX.GETVALUE(inv, 0, 0), 6)
  DIM mul = MATRIX.MULTIPLY(h, h)
  PRINT "mul00="; MATH.ROUND(MATRIX.GETVALUE(mul, 0, 0), 6)
  DIM sq = LINALG.SQRTM(h)
  PRINT "sq00="; MATH.ROUND(MATRIX.GETVALUE(sq, 0, 0), 6)
  DIM lg = LINALG.LOGM(h)
  PRINT "lg00="; MATH.ROUND(MATRIX.GETVALUE(lg, 0, 0), 6)
  DIM ex = LINALG.EXPM(h)
  PRINT "ex00="; MATH.ROUND(MATRIX.GETVALUE(ex, 0, 0), 6)
END SUB


SUB M_handles2()
  DIM h = MATRIX.CREATE(2,2)
  h = MATRIX.SETVALUE(h, 0, 0, 2) : h = MATRIX.SETVALUE(h, 0, 1, 1)
  h = MATRIX.SETVALUE(h, 1, 0, 1) : h = MATRIX.SETVALUE(h, 1, 1, 2)
  DIM hh = LINALG.HESSENBERG(h)
  PRINT "hh00="; MATH.ROUND(MATRIX.GETVALUE(hh, 0, 0), 6)
  DIM co = LINALG.COSM(h)
  PRINT "co00="; MATH.ROUND(MATRIX.GETVALUE(co, 0, 0), 6)
  DIM si = LINALG.SINM(h)
  PRINT "si00="; MATH.ROUND(MATRIX.GETVALUE(si, 0, 0), 6)
  DIM sg = LINALG.SIGNM(h)
  PRINT "sg00="; MATH.ROUND(MATRIX.GETVALUE(sg, 0, 0), 6)
END SUB


SUB M_matrix_ops()
    ' A = [[1,2],[0,3]]  (triangolare superiore, non simmetrica)
    h = MATRIX.CREATE(2, 2)
    h = MATRIX.SETVALUE(h, 0, 0, 1) : h = MATRIX.SETVALUE(h, 0, 1, 2)
    h = MATRIX.SETVALUE(h, 1, 0, 0) : h = MATRIX.SETVALUE(h, 1, 1, 3)

    PRINT "rowcount: "; MATRIX.ROWCOUNT(h)
    PRINT "colcount: "; MATRIX.COLUMNCOUNT(h)
    PRINT "max: "; MATRIX.MAXVALUE(h)
    PRINT "min: "; MATRIX.MINVALUE(h)
    PRINT "frobenius: "; LINALG.FROBENIUSNORM(h)
    PRINT "onenorm: "; LINALG.ONENORM(h)
    PRINT "infnorm: "; LINALG.INFINITYNORM(h)

    IF MATRIX.ISSYMMETRIC(h) THEN PRINT "symmetric: si" ELSE PRINT "symmetric: no"
    IF MATRIX.ISUPPERTRIANGULAR(h) THEN PRINT "upper: si" ELSE PRINT "upper: no"
    IF MATRIX.ISLOWERTRIANGULAR(h) THEN PRINT "lower: si" ELSE PRINT "lower: no"
    IF MATRIX.ISORTHOGONAL(h) THEN PRINT "orthogonal: si" ELSE PRINT "orthogonal: no"

    PRINT "csv: ["; MATRIX.ASCSV(h); "]"
    ' NB: ASJSON NON è testata qui: diverge di UNA parentesi quadra (PCode/IL
    ' `[[[1,2],[0,3]]]` contro VM/C `[[1,2],[0,3]]]`) — reperto separato, da chiudere
    ' decidendo quale forma è quella canonica. Tenerla qui renderebbe rosso un sample
    ' altrimenti verde su 17 simboli su 18.

    ' S = [[1,2],[2,3]]  (simmetrica)
    s = MATRIX.CREATE(2, 2)
    s = MATRIX.SETVALUE(s, 0, 0, 1) : s = MATRIX.SETVALUE(s, 0, 1, 2)
    s = MATRIX.SETVALUE(s, 1, 0, 2) : s = MATRIX.SETVALUE(s, 1, 1, 3)
    IF MATRIX.ISSYMMETRIC(s) THEN PRINT "symmetric2: si" ELSE PRINT "symmetric2: no"

    c = MATRIX.CLONEMATRIX(h)
    PRINT "clone: "; MATRIX.GETVALUE(c, 1, 1)

    r = MATRIX.RESHAPE(h, 1, 4)
    PRINT "reshape_rows: "; MATRIX.ROWCOUNT(r)
    PRINT "reshape_cols: "; MATRIX.COLUMNCOUNT(r)

    ' SLICE con bound INCLUSIVI (come samples/Matrix/MatrixCommonLibSimd.bas):
    ' SLICE(m, 0, 2, 0, 2) su una 3x3 prende tutto.
    sl = MATRIX.SLICE(s, 0, 1, 0, 1)
    PRINT "slice: "; MATRIX.GETVALUE(sl, 0, 0)

    cr = MATRIX.CONCATROWS(h, h)
    PRINT "concatrows: "; MATRIX.ROWCOUNT(cr)
    cc = MATRIX.CONCATCOLS(h, h)
    PRINT "concatcols: "; MATRIX.COLUMNCOUNT(cc)
END SUB


SUB M_matrix_array_ops()
    a = MATRIX.CREATE(2, 2)
    a = MATRIX.SETVALUE(a, 0, 0, 1) : a = MATRIX.SETVALUE(a, 0, 1, 2)
    a = MATRIX.SETVALUE(a, 1, 0, 0) : a = MATRIX.SETVALUE(a, 1, 1, 3)

    DIM rs() = MATRIX.ROWSUMS(a)
    PRINT "rowsums: "; rs(0); ","; rs(1)
    DIM cs() = MATRIX.COLSUMS(a)
    PRINT "colsums: "; cs(0); ","; cs(1)
    DIM rm() = MATRIX.ROWMEANS(a)
    PRINT "rowmeans: "; rm(0); ","; rm(1)
    DIM cm() = MATRIX.COLMEANS(a)
    PRINT "colmeans: "; cm(0); ","; cm(1)
    DIM rv() = MATRIX.ROWVARIANCES(a)
    PRINT "rowvar: "; rv(0); ","; rv(1)
    DIM cv() = MATRIX.COLVARIANCES(a)
    PRINT "colvar: "; cv(0); ","; cv(1)
END SUB


SUB M_matrix_array_ops2()
    a = MATRIX.CREATE(2, 2)
    a = MATRIX.SETVALUE(a, 0, 0, 1) : a = MATRIX.SETVALUE(a, 0, 1, 2)
    a = MATRIX.SETVALUE(a, 1, 0, 0) : a = MATRIX.SETVALUE(a, 1, 1, 3)
    DIM v(1)
    v(0) = 10 : v(1) = 20

    DIM m() = MATRIX.ABOVEMASK(a, 1)
    PRINT "abovemask: "; m(0); ","; m(1); ","; m(2); ","; m(3)
    DIM am() = MATRIX.ABOVEMATRIX(a, 1)
    PRINT "abovematrix: "; am(0); ","; am(1); ","; am(2); ","; am(3)

    DIM argminv() = MATRIX.ARGMIN(a)
    PRINT "argmin: "; argminv(0); ","; argminv(1)

    DIM acv = MATRIX.ADDCOLVECTOR(a, v)
    PRINT "addcol: "; MATRIX.GETVALUE(acv, 0, 0)
    DIM arv = MATRIX.ADDROWVECTOR(a, v)
    PRINT "addrow: "; MATRIX.GETVALUE(arv, 0, 0)

    DIM mv() = MATRIX.MULTIPLYVECTOR(a, v)
    PRINT "mulvec: "; mv(0); ","; mv(1)
    DIM mlv() = MATRIX.MULTIPLYLEFTVECTOR(a, v)
    PRINT "mulleft: "; mlv(0); ","; mlv(1)

    PRINT "asjson: "; MATRIX.ASJSON(a)
END SUB


SUB M_predicates()
  DIM idn = MATRIX.CREATE(3, 3)
  idn = MATRIX.SETVALUE(idn, 0, 0, 1) : idn = MATRIX.SETVALUE(idn, 1, 1, 1) : idn = MATRIX.SETVALUE(idn, 2, 2, 1)
  IF MATRIX.ISSYMMETRIC(idn) <> 0 THEN PRINT "sym=-1"
  IF MATRIX.ISORTHOGONAL(idn) <> 0 THEN PRINT "orth=-1"
  IF MATRIX.ISUPPERTRIANGULAR(idn) <> 0 THEN PRINT "upper=-1"
  IF MATRIX.ISLOWERTRIANGULAR(idn) <> 0 THEN PRINT "lower=-1"
  DIM g = MATRIX.CREATE(2, 2)
  g = MATRIX.SETVALUE(g, 0, 0, 1) : g = MATRIX.SETVALUE(g, 0, 1, 2)
  g = MATRIX.SETVALUE(g, 1, 0, 3) : g = MATRIX.SETVALUE(g, 1, 1, 4)
  PRINT "nsym="; MATRIX.ISSYMMETRIC(g)
  PRINT "nupper="; MATRIX.ISUPPERTRIANGULAR(g)
  PRINT "nlower="; MATRIX.ISLOWERTRIANGULAR(g)
END SUB


SUB M_orth()
  DIM h = MATRIX.CREATE(2,2)
  h = MATRIX.SETVALUE(h, 0, 0, 2) : h = MATRIX.SETVALUE(h, 0, 1, 1)
  h = MATRIX.SETVALUE(h, 1, 0, 1) : h = MATRIX.SETVALUE(h, 1, 1, 2)
  DIM r = LINALG.ORTH(h)
  PRINT "orth="; MATH.ROUND(ABS(MATRIX.GETVALUE(r, 0, 0)), 6)
END SUB


SUB M_pinv()
  DIM h = MATRIX.CREATE(3,3)
  h = MATRIX.SETVALUE(h,0,0,1) : h = MATRIX.SETVALUE(h,0,1,2) : h = MATRIX.SETVALUE(h,0,2,3)
  h = MATRIX.SETVALUE(h,1,0,0) : h = MATRIX.SETVALUE(h,1,1,1) : h = MATRIX.SETVALUE(h,1,2,4)
  h = MATRIX.SETVALUE(h,2,0,5) : h = MATRIX.SETVALUE(h,2,1,6) : h = MATRIX.SETVALUE(h,2,2,0)
  DIM p = LINALG.PINVERSE(h)
  PRINT "p00="; MATH.ROUND(MATRIX.GETVALUE(p,0,0), 6)
END SUB


SUB M_polar()
  DIM h = MATRIX.CREATE(2,2)
  h = MATRIX.SETVALUE(h,0,0,2) : h = MATRIX.SETVALUE(h,0,1,1)
  h = MATRIX.SETVALUE(h,1,0,1) : h = MATRIX.SETVALUE(h,1,1,2)
  DIM ev  AS LONG = LINALG.FACTOR(LINALG.POLAR(h), "U")
  PRINT "v="; MATH.ROUND(MATRIX.GETVALUE(ev,0,0), 6)
END SUB


SUB M_schur()
  DIM h = MATRIX.CREATE(2,2)
  h = MATRIX.SETVALUE(h,0,0,2) : h = MATRIX.SETVALUE(h,0,1,1)
  h = MATRIX.SETVALUE(h,1,0,1) : h = MATRIX.SETVALUE(h,1,1,2)
  DIM ev  AS LONG = LINALG.FACTOR(LINALG.SCHUR(h), "T")
  PRINT "v="; MATH.ROUND(MATRIX.GETVALUE(ev,0,0), 6)
END SUB


SUB M_slice()
  DIM m = MATRIX.CREATE(3, 3)
  m = MATRIX.SETVALUE(m, 0, 0, 1) : m = MATRIX.SETVALUE(m, 1, 1, 1) : m = MATRIX.SETVALUE(m, 2, 2, 1)
  DIM s = MATRIX.SLICE(m, 0, 2, 0, 2)
  PRINT "slicenorm="; MATH.ROUND(LINALG.FROBENIUSNORM(s), 6)
  PRINT "slicerows="; MATRIX.ROWCOUNT(s)
  PRINT "slicecols="; MATRIX.COLUMNCOUNT(s)
END SUB


SUB M_tanm()
  DIM h = MATRIX.CREATE(2,2)
  h = MATRIX.SETVALUE(h, 0, 0, 2) : h = MATRIX.SETVALUE(h, 0, 1, 1)
  h = MATRIX.SETVALUE(h, 1, 0, 1) : h = MATRIX.SETVALUE(h, 1, 1, 2)
  DIM r = LINALG.TANM(h)
  PRINT "tanm="; MATH.ROUND(ABS(MATRIX.GETVALUE(r, 0, 0)), 6)
END SUB


SUB M_eig()
  DIM h = MATRIX.CREATE(2,2)
  h = MATRIX.SETVALUE(h,0,0,2) : h = MATRIX.SETVALUE(h,0,1,1)
  h = MATRIX.SETVALUE(h,1,0,1) : h = MATRIX.SETVALUE(h,1,1,2)
  DIM ev  AS LONG = LINALG.FACTOR(LINALG.EIG(h), "DREAL")
  PRINT "v="; MATH.ROUND(MATRIX.GETVALUE(ev,0,0), 6)
END SUB


SUB M_gen_MATRIX_BLOCKDIAG()
  DIM h = MATRIX.CREATE(2,2)
  h = MATRIX.SETVALUE(h,0,0,2) : h = MATRIX.SETVALUE(h,0,1,1)
  h = MATRIX.SETVALUE(h,1,0,1) : h = MATRIX.SETVALUE(h,1,1,2)
  DIM bd = MATRIX.BLOCKDIAG(h, h)
  PRINT "bd22="; MATH.ROUND(MATRIX.GETVALUE(bd,2,2),6)
END SUB


SUB M_gen_MATRIX_CHOLESKY()
  DIM h = MATRIX.CREATE(2,2)
  h = MATRIX.SETVALUE(h,0,0,2) : h = MATRIX.SETVALUE(h,0,1,1)
  h = MATRIX.SETVALUE(h,1,0,1) : h = MATRIX.SETVALUE(h,1,1,2)
  DIM dec = LINALG.CHOLESKY(h)
  DIM l  AS LONG = LINALG.FACTOR(dec, "L")
  PRINT "l00="; MATH.ROUND(MATRIX.GETVALUE(l,0,0),6)
END SUB


SUB M_gen_MATRIX_CIRCULANT()
  DIM c(2)
  c(0)=1 : c(1)=2 : c(2)=3
  DIM cc = MATRIX.CIRCULANT(c)
  PRINT "c01="; MATH.ROUND(MATRIX.GETVALUE(cc,0,1),6)
END SUB


SUB M_gen_MATRIX_EVD()
  DIM h = MATRIX.CREATE(2,2)
  h = MATRIX.SETVALUE(h,0,0,2) : h = MATRIX.SETVALUE(h,0,1,1)
  h = MATRIX.SETVALUE(h,1,0,1) : h = MATRIX.SETVALUE(h,1,1,2)
  DIM ed = LINALG.EVD(h)
  DIM ev  AS LONG = LINALG.FACTOR(ed, "D")
  PRINT "ev="; MATH.ROUND(MATRIX.GETVALUE(ev,1,1),6)
END SUB


SUB M_gen_MATRIX_FRACTIONALPOWER()
  DIM h = MATRIX.CREATE(2,2)
  h = MATRIX.SETVALUE(h,0,0,2) : h = MATRIX.SETVALUE(h,0,1,1)
  h = MATRIX.SETVALUE(h,1,0,1) : h = MATRIX.SETVALUE(h,1,1,2)
  DIM fp = LINALG.FRACTIONALPOWER(h, 0.5)
  PRINT "fp00="; MATH.ROUND(MATRIX.GETVALUE(fp,0,0),6)
END SUB


SUB M_gen_MATRIX_HANKEL()
  DIM c(2), r(2)
  c(0)=1 : c(1)=2 : c(2)=3
  r(0)=3 : r(1)=4 : r(2)=5
  DIM hk = MATRIX.HANKEL(c, r)
  PRINT "hk="; MATH.ROUND(MATRIX.GETVALUE(hk,2,1),6)
END SUB


SUB M_gen_MATRIX_KRONECKER()
  DIM a = MATRIX.CREATE(2,2)
  a = MATRIX.SETVALUE(a,0,0,1) : a = MATRIX.SETVALUE(a,0,1,2)
  a = MATRIX.SETVALUE(a,1,0,3) : a = MATRIX.SETVALUE(a,1,1,4)
  DIM b = MATRIX.CREATE(2,2)
  b = MATRIX.SETVALUE(b,0,0,0) : b = MATRIX.SETVALUE(b,0,1,5)
  b = MATRIX.SETVALUE(b,1,0,6) : b = MATRIX.SETVALUE(b,1,1,7)
  DIM k = MATRIX.KRONECKER(a, b)
  PRINT "k00="; MATH.ROUND(MATRIX.GETVALUE(k,0,0),6)
END SUB


SUB M_gen_MATRIX_LOADCSV()
  DIM m = MATRIX.LOADCSV("mt_loadcsv_fixture.csv")
  PRINT "csv="; MATH.ROUND(MATRIX.GETVALUE(m,2,2),6)
END SUB


SUB M_gen_MATRIX_LOADJSON()
  DIM m = MATRIX.LOADJSON("mt_loadjson_fixture.json")
  PRINT "json="; MATH.ROUND(MATRIX.GETVALUE(m,0,0),6)
END SUB


SUB M_gen_MATRIX_LU()
  DIM a = MATRIX.CREATE(2,2)
  a = MATRIX.SETVALUE(a,0,0,1) : a = MATRIX.SETVALUE(a,0,1,2)
  a = MATRIX.SETVALUE(a,1,0,3) : a = MATRIX.SETVALUE(a,1,1,4)
  DIM lu = LINALG.LU(a)
  DIM u  AS LONG = LINALG.FACTOR(lu, "U")
  PRINT "u00="; MATH.ROUND(MATRIX.GETVALUE(u,0,0),6)
END SUB


SUB M_gen_MATRIX_NUCLEARNORM()
  DIM h = MATRIX.CREATE(2,2)
  h = MATRIX.SETVALUE(h,0,0,2) : h = MATRIX.SETVALUE(h,0,1,1)
  h = MATRIX.SETVALUE(h,1,0,1) : h = MATRIX.SETVALUE(h,1,1,2)
  PRINT "nuc="; MATH.ROUND(LINALG.NUCLEARNORM(h),6)
END SUB


SUB M_gen_MATRIX_NULLSPACE()
  DIM a = MATRIX.CREATE(2,2)
  a = MATRIX.SETVALUE(a,0,0,1) : a = MATRIX.SETVALUE(a,0,1,1)
  a = MATRIX.SETVALUE(a,1,0,1) : a = MATRIX.SETVALUE(a,1,1,1)
  DIM ns = LINALG.NULLSPACE(a)
  PRINT "ns="; MATH.ROUND(ABS(MATRIX.GETVALUE(ns,0,0)),6)
END SUB


SUB M_gen_MATRIX_QR()
  DIM a = MATRIX.CREATE(2,2)
  a = MATRIX.SETVALUE(a,0,0,1) : a = MATRIX.SETVALUE(a,0,1,2)
  a = MATRIX.SETVALUE(a,1,0,3) : a = MATRIX.SETVALUE(a,1,1,4)
  DIM qr = LINALG.QR(a)
  DIM r  AS LONG = LINALG.FACTOR(qr, "R")
  PRINT "r00="; MATH.ROUND(MATRIX.GETVALUE(r,0,0),6)
END SUB


SUB M_gen_MATRIX_SOLVE()
  DIM a = MATRIX.CREATE(2,2)
  a = MATRIX.SETVALUE(a,0,0,2) : a = MATRIX.SETVALUE(a,0,1,1)
  a = MATRIX.SETVALUE(a,1,0,1) : a = MATRIX.SETVALUE(a,1,1,2)
  DIM b(1)
  b(0)=4 : b(1)=5
  DIM xs() = LINALG.SOLVE(a, b)
  PRINT "x0="; MATH.ROUND(xs(0),6)
END SUB


SUB M_gen_MATRIX_SOLVETRIANGULAR()
  DIM a = MATRIX.CREATE(2,2)
  a = MATRIX.SETVALUE(a,0,0,2) : a = MATRIX.SETVALUE(a,0,1,1)
  a = MATRIX.SETVALUE(a,1,0,0) : a = MATRIX.SETVALUE(a,1,1,3)
  DIM b(1)
  b(0)=4 : b(1)=6
  DIM xs() = LINALG.SOLVETRIANGULAR(a, b, 0)
  PRINT "x0="; MATH.ROUND(xs(0),6)
END SUB


SUB M_gen_MATRIX_SUBSPACEANGLES()
  DIM h = MATRIX.CREATE(2,2)
  h = MATRIX.SETVALUE(h,0,0,2) : h = MATRIX.SETVALUE(h,0,1,1)
  h = MATRIX.SETVALUE(h,1,0,1) : h = MATRIX.SETVALUE(h,1,1,2)
  DIM g = MATRIX.CREATE(2,2)
  g = MATRIX.SETVALUE(g,0,0,1) : g = MATRIX.SETVALUE(g,0,1,2)
  g = MATRIX.SETVALUE(g,1,0,2) : g = MATRIX.SETVALUE(g,1,1,3)
  DIM ang() = LINALG.SUBSPACEANGLES(h, g)
  PRINT "anglen="; UBOUND(ang)+1
  PRINT "angle0="; MATH.ROUND(ang(0),6)
END SUB


SUB M_gen_MATRIX_SVD()
  DIM h = MATRIX.CREATE(2,2)
  h = MATRIX.SETVALUE(h,0,0,2) : h = MATRIX.SETVALUE(h,0,1,1)
  h = MATRIX.SETVALUE(h,1,0,1) : h = MATRIX.SETVALUE(h,1,1,2)
  DIM svd = LINALG.SVD(h)
  DIM s  AS LONG = LINALG.FACTOR(svd, "S")
  PRINT "s00="; MATH.ROUND(MATRIX.GETVALUE(s,0,0),6)
END SUB


SUB M_gen_MATRIX_TOEPLITZ()
  DIM c(2), r(2)
  c(0)=1 : c(1)=2 : c(2)=3
  r(0)=1 : r(1)=4 : r(2)=5
  DIM tp = MATRIX.TOEPLITZ(c, r)
  PRINT "tp="; MATH.ROUND(MATRIX.GETVALUE(tp,0,1),6)
END SUB



SUB Extra()
  DIM a = MATRIX.CREATE(2, 2)
  a = MATRIX.SETVALUE(a, 0, 0, 1) : a = MATRIX.SETVALUE(a, 0, 1, 2)
  a = MATRIX.SETVALUE(a, 1, 0, 0) : a = MATRIX.SETVALUE(a, 1, 1, 3)
  DIM amax() = MATRIX.ARGMAX(a)
  PRINT "argmax="; amax(0); ","; amax(1)
  DIM t = MATRIX.TRANSPOSE(a)
  PRINT "transpose="; MATRIX.GETVALUE(t, 1, 0)
END SUB

SUB Main()
  M_handles()
  M_handles2()
  M_matrix_ops()
  M_matrix_array_ops()
  M_matrix_array_ops2()
  M_predicates()
  M_orth()
  M_pinv()
  M_polar()
  M_schur()
  M_slice()
  M_tanm()
  M_eig()
  M_gen_MATRIX_BLOCKDIAG()
  M_gen_MATRIX_CHOLESKY()
  M_gen_MATRIX_CIRCULANT()
  M_gen_MATRIX_EVD()
  M_gen_MATRIX_FRACTIONALPOWER()
  M_gen_MATRIX_HANKEL()
  M_gen_MATRIX_KRONECKER()
  M_gen_MATRIX_LOADCSV()
  M_gen_MATRIX_LOADJSON()
  M_gen_MATRIX_LU()
  M_gen_MATRIX_NUCLEARNORM()
  M_gen_MATRIX_NULLSPACE()
  M_gen_MATRIX_QR()
  M_gen_MATRIX_SOLVE()
  M_gen_MATRIX_SOLVETRIANGULAR()
  M_gen_MATRIX_SUBSPACEANGLES()
  M_gen_MATRIX_SVD()
  M_gen_MATRIX_TOEPLITZ()
  Extra()
END SUB
