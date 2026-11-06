' UDT con DUE campi ARRAY numerici (S3b): layout C contiguo, passaggio per valore e ritorno.
' EXPECT: sz= 56|s= 31|a0= 2|a4= 6|d0= 1.5|d2= 12.5|b= 26
TYPE ArrMix
    A(4) AS LONG
    D(2) AS DOUBLE
    B AS DOUBLE
END TYPE

DECLARE FUNCTION ArrMixSize LIB "liblayout_ext" ALIAS "arrmix_size" () AS LONG
DECLARE FUNCTION ArrMixSum LIB "liblayout_ext" ALIAS "arrmix_sum" (v AS ArrMix) AS DOUBLE
DECLARE FUNCTION ArrMixBump LIB "liblayout_ext" ALIAS "arrmix_bump" (v AS ArrMix) AS ArrMix

SUB Main()
    PRINT "sz="; ArrMixSize()
    DIM v AS ArrMix
    v.A(0) = 1 : v.A(4) = 5
    v.D(0) = 1 : v.D(2) = 12
    v.B = 1
    PRINT "s="; ArrMixSum(v)

    DIM r AS ArrMix
    r = ArrMixBump(v)
    PRINT "a0="; r.A(0)
    PRINT "a4="; r.A(4)
    PRINT "d0="; r.D(0)
    PRINT "d2="; r.D(2)
    PRINT "b="; r.B
END SUB
