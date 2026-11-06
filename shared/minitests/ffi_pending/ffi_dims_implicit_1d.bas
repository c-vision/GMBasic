' @heavy   (pending: richiede le DIMENSIONI IMPLICITE degli array nella DECLARE FFI)
' Una volta implementato il descrittore {rank, extents} nel valore array, togliere @heavy
' e spostare il file in ffi/.
' 1-D: la DECLARE non elenca piu' n; il compilatore emette d0 = UBOUND(a,0)+1 dopo il puntatore.
' EXPECT: mean= 2.5
DECLARE FUNCTION MEAN1D LIB "gmbstats" ALIAS "gs_stats_mean" (a() AS DOUBLE) AS DOUBLE

SUB Main()
    DIM m(3) AS DOUBLE
    m(0) = 1 : m(1) = 2 : m(2) = 3 : m(3) = 4
    PRINT "mean="; MEAN1D(m)
END SUB
