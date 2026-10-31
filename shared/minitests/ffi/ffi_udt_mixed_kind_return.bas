' EXPECT: a.V= 3.5|a.N= 7|b.N= 9|b.V= 2.25
TYPE LabeledDN
    V AS DOUBLE
    N AS LONG
END TYPE

TYPE LabeledND
    N AS LONG
    V AS DOUBLE
END TYPE

DECLARE FUNCTION VecMakeLabeledDN LIB "libvec_module" ALIAS "vec_make_labeled_dn" (v AS DOUBLE, n AS DOUBLE) AS LabeledDN
DECLARE FUNCTION VecMakeLabeledND LIB "libvec_module" ALIAS "vec_make_labeled_nd" (n AS DOUBLE, v AS DOUBLE) AS LabeledND

SUB Main()
    DIM a AS LabeledDN
    a = VecMakeLabeledDN(3.5, 7)
    PRINT "a.V="; a.V
    PRINT "a.N="; a.N

    DIM b AS LabeledND
    b = VecMakeLabeledND(9, 2.25)
    PRINT "b.N="; b.N
    PRINT "b.V="; b.V
END SUB
