' EXPECT: mixed= 15.5|mixed2= 6.5
' Array BYREF + scalari DOUBLE nella stessa firma (2026-11-02): il puntatore forza Int64,
' gli scalari restano DOUBLE — kind PER SLOT (scalarKindMask), non più firma omogenea.
DECLARE FUNCTION VecArrScale LIB "libvec_module" ALIAS "vec_array_scale" (buf() AS DOUBLE, n AS LONG, scale AS DOUBLE, offset AS DOUBLE) AS DOUBLE

SUB Main()
    DIM v(2) AS DOUBLE
    v(0) = 1.0 : v(1) = 2.0 : v(2) = 3.0
    PRINT "mixed="; VecArrScale(v, 3, 2.5, 0.5)
    PRINT "mixed2="; VecArrScale(v, 2, 2.0, 0.5)
END SUB
