' EXPECT: total= 20
' Array E STRING nella STESSA firma (combinazione non coperta dagli altri minitest
' FFI, entrambi puntatori/firma INT64): somma il buffer piu' la lunghezza del prefisso.
DECLARE FUNCTION VecLabelLen LIB "libvec_module" ALIAS "vec_label_len" (prefix AS STRING, buf() AS LONG, n AS LONG) AS LONG

SUB Main()
    DIM v(5) AS LONG
    v(0) = 1 : v(1) = 2 : v(2) = 3 : v(3) = 4 : v(4) = 5
    PRINT "total="; VecLabelLen("abcde", v, 5)   ' len("abcde")=5 + sum(1..5)=15 = 20
END SUB
