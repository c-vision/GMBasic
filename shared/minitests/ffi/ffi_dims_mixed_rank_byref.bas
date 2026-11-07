' Come ffi_dims_mixed_rank, ma la FFI ha anche un parametro BYREF scalare: il rango misto
' attraversa il call-site BYREF (scelta della variante a runtime dai bound).
' EXPECT: v= 5|m= 3|v2= 5|m2= 3
DECLARE SUB FirstExtOut LIB "libdims_ext" ALIAS "first_ext_out" (BYREF o AS DOUBLE, a() AS DOUBLE)

FUNCTION Fwd(m() AS DOUBLE) AS DOUBLE
    DIM r AS DOUBLE
    FirstExtOut(r, m)
    RETURN r
END FUNCTION

SUB Main()
    DIM v(4) AS DOUBLE
    DIM m(2, 3) AS DOUBLE
    PRINT "v="; Fwd(v)
    PRINT "m="; Fwd(m)
    PRINT "v2="; Fwd(v)
    PRINT "m2="; Fwd(m)
END SUB
