' EXPECT: pv= 10
' BYREF scalare elemento DOUBLE (2026-09-28, §1bis), IN/OUT — il valore iniziale viene
' letto E riscritto dalla funzione nativa. Il parametro "n" resta LONG: il puntatore
' forza comunque la firma a INT64 indipendentemente dal tipo puntato (stessa regola
' degli array BYREF, §4bis).
DECLARE SUB VecScaleRef LIB "libvec_module" ALIAS "vec_scale_ref" (n AS LONG, BYREF pv AS DOUBLE)

SUB Main()
    DIM pv AS DOUBLE
    pv = 2.5
    VecScaleRef(4, pv)
    PRINT "pv="; pv
END SUB
