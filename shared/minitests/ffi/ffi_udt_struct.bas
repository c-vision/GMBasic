' EXPECT: p1.X= 3|p1.Y= 4|len= 5|p3.X= 4|p3.Y= 5|i3.A= 11|i3.B= 22
TYPE Point
    X AS DOUBLE
    Y AS DOUBLE
END TYPE

TYPE Int2
    A AS LONG
    B AS LONG
END TYPE

DECLARE FUNCTION VecMakePoint LIB "libvec_module" ALIAS "vec_make_point" (x AS DOUBLE, y AS DOUBLE) AS Point
DECLARE FUNCTION VecPointLen LIB "libvec_module" ALIAS "vec_point_len" (p AS Point) AS DOUBLE
DECLARE FUNCTION VecPointAdd LIB "libvec_module" ALIAS "vec_point_add" (a AS Point, b AS Point) AS Point
DECLARE FUNCTION VecInt2Sum LIB "libvec_module" ALIAS "vec_int2_sum" (a AS Int2, b AS Int2) AS Int2

SUB Main()
    DIM p1 AS Point
    p1 = VecMakePoint(3, 4)
    PRINT "p1.X="; p1.X
    PRINT "p1.Y="; p1.Y
    PRINT "len="; VecPointLen(p1)

    DIM p2 AS Point
    p2 = VecMakePoint(1, 1)
    DIM p3 AS Point
    p3 = VecPointAdd(p1, p2)
    PRINT "p3.X="; p3.X
    PRINT "p3.Y="; p3.Y

    DIM i1 AS Int2
    i1.A = 1 : i1.B = 2
    DIM i2 AS Int2
    i2.A = 10 : i2.B = 20
    DIM i3 AS Int2
    i3 = VecInt2Sum(i1, i2)
    PRINT "i3.A="; i3.A
    PRINT "i3.B="; i3.B
END SUB
