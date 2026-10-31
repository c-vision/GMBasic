' EXPECT: r= 17.5|s= 11|r8= 36
' Firma mista LONG/DOUBLE fra scalari semplici (2026-10): nessun array/STRING/BYREF/UDT/
' callback coinvolto — solo scalari intercalati, kind diverso anche sul ritorno. Prima di
' questo supporto era un errore di compilazione esplicito ("firma mista LONG/DOUBLE non
' supportata dal marshalling FFI"). Copre: FUNCTION con ritorno DOUBLE e parametri misti
' (vec_mixed), SUB con BYREF per il risultato (vec_mixed_sub), e il caso limite 8 parametri
' alternati LONG/DOUBLE con ritorno LONG (vec_mixed8 — esercita il dispatcher generato oltre
' la shape minima).
DECLARE FUNCTION VecMixed LIB "libvec_module" ALIAS "vec_mixed" (id AS LONG, price AS DOUBLE, qty AS LONG) AS DOUBLE
DECLARE SUB VecMixedSub LIB "libvec_module" ALIAS "vec_mixed_sub" (a AS LONG, b AS DOUBLE)
DECLARE FUNCTION VecMixedSubLast LIB "libvec_module" ALIAS "vec_mixed_sub_last" () AS LONG
DECLARE FUNCTION VecMixed8 LIB "libvec_module" ALIAS "vec_mixed8" (a AS LONG, b AS DOUBLE, c AS LONG, d AS DOUBLE, e AS LONG, f AS DOUBLE, g AS LONG, h AS DOUBLE) AS LONG

SUB Main()
    PRINT "r="; VecMixed(7, 2.5, 7)

    VecMixedSub(5, 6.4)
    PRINT "s="; VecMixedSubLast()

    PRINT "r8="; VecMixed8(1, 2.0, 3, 4.0, 5, 6.0, 7, 8.0)
END SUB
