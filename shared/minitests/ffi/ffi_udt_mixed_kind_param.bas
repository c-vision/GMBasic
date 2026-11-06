' EXPECT: dn_total= 10.5|nd_total= 11.25|dn_between= 1110.5|dn_last= 16.5|dn_with_array= 70
TYPE LabeledDN
    V AS DOUBLE
    N AS LONG
END TYPE
TYPE LabeledND
    N AS LONG
    V AS DOUBLE
END TYPE
DECLARE FUNCTION VecLabeledDnTotal LIB "libvec_module" ALIAS "vec_labeled_dn_total" (a AS LabeledDN) AS DOUBLE
DECLARE FUNCTION VecLabeledNdTotal LIB "libvec_module" ALIAS "vec_labeled_nd_total" (a AS LabeledND) AS DOUBLE
DECLARE FUNCTION VecLabeledDnBetween LIB "libvec_module" ALIAS "vec_labeled_dn_between" (before AS DOUBLE, a AS LabeledDN, after AS DOUBLE) AS DOUBLE
DECLARE FUNCTION VecLabeledDnLast LIB "libvec_module" ALIAS "vec_labeled_dn_last" (a AS DOUBLE, b AS DOUBLE, c AS DOUBLE, lab AS LabeledDN) AS DOUBLE
DECLARE FUNCTION VecLabeledDnWithArray LIB "libvec_module" ALIAS "vec_labeled_dn_with_array" (buf() AS LONG, lab AS LabeledDN) AS LONG

SUB Main()
    DIM a AS LabeledDN
    a.V = 3.5 : a.N = 7
    PRINT "dn_total="; VecLabeledDnTotal(a)   ' 10.5

    DIM b AS LabeledND
    b.N = 9 : b.V = 2.25
    PRINT "nd_total="; VecLabeledNdTotal(b)   ' 11.25

    PRINT "dn_between="; VecLabeledDnBetween(100.0, a, 1000.0)  ' 1110.5

    PRINT "dn_last="; VecLabeledDnLast(1.0, 2.0, 3.0, a)   ' 13.5

    DIM buf(2) AS LONG
    buf(0) = 10 : buf(1) = 20 : buf(2) = 30
    PRINT "dn_with_array="; VecLabeledDnWithArray(buf(), a)  ' 60+3+7=70
END SUB
