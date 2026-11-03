' EXPECT: root= 2|root2= 1.414214
' Callback SCALARE + limiti DOUBLE nella stessa firma (2026-11-02): forma reale di
' OPTIMIZE.BRENTQ. Il callback è un puntatore (Int64), a/b sono DOUBLE — kind per-slot.
IMPORT MATH
DECLARE FUNCTION VecCbRootq LIB "libvec_module" ALIAS "vec_cb_rootq" (cb AS FUNCTION(x AS DOUBLE) AS DOUBLE, a AS DOUBLE, b AS DOUBLE) AS DOUBLE

SUB Main()
    PRINT "root="; MATH.ROUND(VecCbRootq(FN(x AS DOUBLE) AS DOUBLE => x * x - 4.0, 0.0, 5.0), 6)
    PRINT "root2="; MATH.ROUND(VecCbRootq(FN(x AS DOUBLE) AS DOUBLE => x * x - 2.0, 0.0, 5.0), 6)
END SUB
