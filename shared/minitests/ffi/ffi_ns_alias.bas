' EXPECT: a= 6|b= 12
IMPORT FFINS AS F

SUB Main()
    PRINT "a="; F.vec_scale(1.5, 4.0)
    PRINT "b="; F.ns_scale(3.0, 4.0)
END SUB