' Una SUB con parametro a() che riceve array di RANGHI DIVERSI (1-D e 2-D) e li inoltra alla FFI:
' il rango viaggia col valore (IL, PCode, VM/C) o, su C99, e' un rango misto (bound ABS per la
' dimensione assente) e la chiamata sceglie la variante a runtime. first_ext(a, d0) ignora le altre estensioni.
' EXPECT: v= 5|m= 3|v2= 5|m2= 3
DECLARE FUNCTION FirstExt LIB "libdims_ext" ALIAS "first_ext" (a() AS DOUBLE) AS DOUBLE

FUNCTION Fwd(m() AS DOUBLE) AS DOUBLE
    RETURN FirstExt(m)
END FUNCTION

SUB Main()
    DIM v(4) AS DOUBLE
    DIM m(2, 3) AS DOUBLE
    PRINT "v="; Fwd(v)
    PRINT "m="; Fwd(m)
    PRINT "v2="; Fwd(v)
    PRINT "m2="; Fwd(m)
END SUB
