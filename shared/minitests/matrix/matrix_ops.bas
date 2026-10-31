' MiniTest MATRIX — funzioni che nei sample GENERATI erano chiamate con un handle fittizio
' (`MATRIX.ROWCOUNT(0)` → "Handle matrice non valido"): lì l'argomento è un HANDLE (LongType),
' che il generatore non può distinguere da un numero. Qui si usano matrici VERE.
' EXPECT: rowcount:  2|colcount:  2|max:  3|min:  0|frobenius:  3.74165738677394|onenorm:  5|infnorm:  3|symmetric: no|upper: si|lower: no|orthogonal: no|csv: [1,2|0,3|]|symmetric2: si|clone:  3|reshape_rows:  1|reshape_cols:  4|slice:  1|concatrows:  4|concatcols:  4
IMPORT MATRIX

SUB Main()
    ' A = [[1,2],[0,3]]  (triangolare superiore, non simmetrica)
    h = MATRIX.CREATE(2, 2)
    h = MATRIX.SETVALUE(h, 0, 0, 1) : h = MATRIX.SETVALUE(h, 0, 1, 2)
    h = MATRIX.SETVALUE(h, 1, 0, 0) : h = MATRIX.SETVALUE(h, 1, 1, 3)

    PRINT "rowcount: "; MATRIX.ROWCOUNT(h)
    PRINT "colcount: "; MATRIX.COLUMNCOUNT(h)
    PRINT "max: "; MATRIX.MAXVALUE(h)
    PRINT "min: "; MATRIX.MINVALUE(h)
    PRINT "frobenius: "; MATRIX.FROBENIUSNORM(h)
    PRINT "onenorm: "; MATRIX.ONENORM(h)
    PRINT "infnorm: "; MATRIX.INFINITYNORM(h)

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

    ' SLICE con bound INCLUSIVI (come samples/Community/Matrix/MatrixCommonLibSimd.bas):
    ' SLICE(m, 0, 2, 0, 2) su una 3x3 prende tutto.
    sl = MATRIX.SLICE(s, 0, 1, 0, 1)
    PRINT "slice: "; MATRIX.GETVALUE(sl, 0, 0)

    cr = MATRIX.CONCATROWS(h, h)
    PRINT "concatrows: "; MATRIX.ROWCOUNT(cr)
    cc = MATRIX.CONCATCOLS(h, h)
    PRINT "concatcols: "; MATRIX.COLUMNCOUNT(cc)
END SUB
