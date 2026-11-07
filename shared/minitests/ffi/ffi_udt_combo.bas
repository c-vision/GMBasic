' Combinazione: struct ANNIDATA + campo ARRAY + larghezze miste, parametro e ritorno (S3b).
' EXPECT: sz= 64|s= 38|px= 2|py= 4|k= 6|z= 8|a0= 9|a2= 19|n= 7
TYPE Pt2
    X AS DOUBLE
    Y AS DOUBLE
END TYPE
TYPE Blob
    P AS Pt2
    K AS LONG
    Z AS DOUBLE
    A(2) AS DOUBLE
    N AS QUAD
END TYPE

DECLARE FUNCTION BlobSize LIB "liblayout_ext" ALIAS "blob_size" () AS LONG
DECLARE FUNCTION BlobSum  LIB "liblayout_ext" ALIAS "blob_sum" (b AS Blob) AS DOUBLE
DECLARE FUNCTION BlobBump LIB "liblayout_ext" ALIAS "blob_bump" (b AS Blob) AS Blob

SUB Main()
    PRINT "sz="; BlobSize()
    DIM b AS Blob
    b.P.X = 1 : b.P.Y = 2 : b.K = 3 : b.Z = 4
    b.A(0) = 4 : b.A(1) = 9 : b.A(2) = 14
    b.N = 1
    PRINT "s="; BlobSum(b)

    DIM r AS Blob
    r = BlobBump(b)
    PRINT "px="; r.P.X
    PRINT "py="; r.P.Y
    PRINT "k="; r.K
    PRINT "z="; r.Z
    PRINT "a0="; r.A(0)
    PRINT "a2="; r.A(2)
    PRINT "n="; r.N
END SUB
