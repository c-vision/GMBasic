' EXPECT: curve= 247||patch= 839||field= 353||scale= 4|: 2.5 , 5 , 7.5 , 10||lam= 4|: 11 , 12 , 13 , 14||shadow= 257
' Callback "campi" (2026-11-07): le due forme che NON hanno array in testa.
'   148  f(v(), x) AS ARRAY     -> double (*)(double*, int64_t, double, double*)
'   193  f(t) AS ARRAY          -> double (*)(double, double*)
'   194  f(u, v) AS ARRAY       -> double (*)(double, double, double*)
'   195  f(x, y, z) AS ARRAY    -> double (*)(double, double, double, double*)
' Sono le forme usate da VECCALC (campo vettoriale = 195, curva = 193, patch = 194)
' e da OPTIMIZE (148). Il valore di ritorno nativo e' il numero di elementi scritti.
DECLARE FUNCTION CbfCurve LIB "libcbfield_ext" ALIAS "cbf_curve193" (f AS FUNCTION(t AS DOUBLE) AS ARRAY, t AS DOUBLE) AS DOUBLE
DECLARE FUNCTION CbfPatch LIB "libcbfield_ext" ALIAS "cbf_patch194" (f AS FUNCTION(u AS DOUBLE, v AS DOUBLE) AS ARRAY, u AS DOUBLE, v AS DOUBLE) AS DOUBLE
DECLARE FUNCTION CbfField LIB "libcbfield_ext" ALIAS "cbf_field195" (f AS FUNCTION(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS ARRAY, x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS DOUBLE
DECLARE FUNCTION CbfScale LIB "libcbfield_ext" ALIAS "cbf_scale148" (f AS FUNCTION(v() AS DOUBLE, s AS DOUBLE) AS ARRAY, v() AS DOUBLE, s AS DOUBLE, out() AS DOUBLE) AS DOUBLE

' 193: r(t) = (t, 2t, 3t+1)  -> a t=2: (2,4,7)
FUNCTION Helix(t AS DOUBLE) AS ARRAY OF DOUBLE
    RETURN {t, t * 2.0, t * 3.0 + 1.0}
END FUNCTION

' 194: S(u,v) = (u+v, u*v, u+v+u*v) -> a (3,4): (7,12,19)
FUNCTION PatchF(u AS DOUBLE, v AS DOUBLE) AS ARRAY OF DOUBLE
    RETURN {u + v, u * v, u + v + u * v}
END FUNCTION

' 195: F(x,y,z) = (x+y, y+z, x*z) -> a (1,2,3): (3,5,3)
FUNCTION Field(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS ARRAY OF DOUBLE
    RETURN {x + y, y + z, x * z}
END FUNCTION

' 148: out(i) = v(i) * s
FUNCTION Scaled(v() AS DOUBLE, s AS DOUBLE) AS ARRAY OF DOUBLE
    RETURN {v(0) * s, v(1) * s, v(2) * s, v(3) * s}
END FUNCTION

SUB Main()
    DIM i AS LONG

    PRINT "curve="; CbfCurve(Helix, 2)
    PRINT "|patch="; CbfPatch(PatchF, 3, 4)
    PRINT "|field="; CbfField(Field, 1, 2, 3)

    DIM v(3) AS DOUBLE
    FOR i = 0 TO 3
        v(i) = i + 1
    NEXT i
    DIM o(3) AS DOUBLE
    PRINT "|scale="; CbfScale(Scaled, v, 2.5, o)
    PRINT ":"; o(0); ","; o(1); ","; o(2); ","; o(3)

    ' lambda al posto di una FUNCTION nominata (stessa ABI)
    DIM q(3) AS DOUBLE
    PRINT "|lam="; CbfScale(FN(w() AS DOUBLE, k AS DOUBLE) AS ARRAY OF DOUBLE => {w(0) + k, w(1) + k, w(2) + k, w(3) + k}, v, 10.0, q)
    PRINT ":"; q(0); ","; q(1); ","; q(2); ","; q(3)

    ' REGRESSIONE 2026-11-07: il parametro `v` della lambda OMBRA l'array esterno `v`.
    ' Prima, sul ramo upvalue (PCodeVM), la lambda leggeva l'array esterno invece del
    ' parametro: il callback FFI falliva con InvalidCastException. Atteso 257 = 2*100+5*10+7.
    PRINT "|shadow="; CbfField(FN(v AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS ARRAY OF DOUBLE => {v, y, z}, 2.0, 5.0, 7.0)
END SUB
