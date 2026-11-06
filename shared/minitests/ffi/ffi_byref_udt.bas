' BYREF UDT (2026-11-06): `BYREF p AS <TYPE>` → `<Udt>_t*` (in/out di struct) su 4 gambe.
' EXPECT: s= 5|x= 4|y= 6|x2= 10|y2= 20|s2= 30
TYPE ByrefPt
    X AS DOUBLE
    Y AS DOUBLE
END TYPE

DECLARE FUNCTION ByrefPtSum   LIB "liblayout_ext" ALIAS "byref_pt_sum"   (BYREF p AS ByrefPt) AS DOUBLE
DECLARE SUB      ByrefPtScale LIB "liblayout_ext" ALIAS "byref_pt_scale" (BYREF p AS ByrefPt, k AS DOUBLE)
DECLARE SUB      ByrefPtSet   LIB "liblayout_ext" ALIAS "byref_pt_set"   (BYREF p AS ByrefPt, x AS DOUBLE, y AS DOUBLE)

SUB Main()
    DIM p AS ByrefPt
    p.X = 2 : p.Y = 3
    PRINT "s="; ByrefPtSum(p)

    ByrefPtScale(p, 2)
    PRINT "x="; p.X
    PRINT "y="; p.Y

    ByrefPtSet(p, 10, 20)
    PRINT "x2="; p.X
    PRINT "y2="; p.Y
    PRINT "s2="; ByrefPtSum(p)
END SUB
