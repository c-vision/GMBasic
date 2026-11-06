' UDT con layout C reale (S1, 2026-11-06): campi a 1/8 byte, padding, kind misto.
' Le size/offset sono confrontate con quelle calcolate dal compilatore C.
' EXPECT: sz= 24|ob= 8|oc= 16|fsz= 16|s1= 13|b.a= 4|b.b= 5.5|b.c= 6|f1= 6.5|fk.x= 2.5|fk.y= 5|fk.z= 5
TYPE Mixed
    A AS LONG
    B AS DOUBLE
    C AS BYTE
END TYPE

TYPE Fiz
    X AS SINGLE
    Y AS INTEGER
    Z AS QUAD
END TYPE

DECLARE FUNCTION MixedSize LIB "liblayout_ext" ALIAS "mixed_size" () AS LONG
DECLARE FUNCTION MixedOffB LIB "liblayout_ext" ALIAS "mixed_off_b" () AS LONG
DECLARE FUNCTION MixedOffC LIB "liblayout_ext" ALIAS "mixed_off_c" () AS LONG
DECLARE FUNCTION FizSize LIB "liblayout_ext" ALIAS "fiz_size" () AS LONG
DECLARE FUNCTION MixedSum LIB "liblayout_ext" ALIAS "mixed_sum" (m AS Mixed) AS DOUBLE
DECLARE FUNCTION MixedBump LIB "liblayout_ext" ALIAS "mixed_bump" (m AS Mixed) AS Mixed
DECLARE FUNCTION FizSum LIB "liblayout_ext" ALIAS "fiz_sum" (f AS Fiz) AS DOUBLE
DECLARE FUNCTION FizBump LIB "liblayout_ext" ALIAS "fiz_bump" (f AS Fiz) AS Fiz

SUB Main()
    PRINT "sz="; MixedSize()
    PRINT "ob="; MixedOffB()
    PRINT "oc="; MixedOffC()
    PRINT "fsz="; FizSize()
    
    DIM m AS Mixed
    m.A = 3 : m.B = 5 : m.C = 5
    PRINT "s1="; MixedSum(m)

    DIM b AS Mixed
    b = MixedBump(m)
    PRINT "b.a="; b.A
    PRINT "b.b="; b.B
    PRINT "b.c="; b.C

    DIM f AS Fiz
    f.X = 1.5 : f.Y = 3 : f.Z = 2
    PRINT "f1="; FizSum(f)

    DIM fk AS Fiz
    fk = FizBump(f)
    PRINT "fk.x="; fk.X
    PRINT "fk.y="; fk.Y
    PRINT "fk.z="; fk.Z
END SUB
