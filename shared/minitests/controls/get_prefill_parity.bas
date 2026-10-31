' TAG: interactive
' @GET prefill (Clipper): il campo parte con il valore CORRENTE della variabile e il
' cursore e' in coda, quindi i tasti si aggiungono ("ab" + "cd" = "abcd").
' EXPECT: a=abcd
DYNAMIC a AS STRING = "ab"
@ 2, 10 GET a
READ
PRINT "a="; a
