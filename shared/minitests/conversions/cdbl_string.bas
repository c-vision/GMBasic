' MiniTest CDBL/CSNG da STRINGA numerica (4 backend): il testo va parsato, non emesso.
' Stessa forma del difetto CCUR (conversions/ccur_string.bas): sul C99Native i due token
' erano `cv_identity`, quindi una gmb_string_t finiva in contesto numerico e il C non
' compilava ("invalid operands to binary expression").
' EXPECT: cdbl-s|csng-s|cdbl-neg|cdbl-n|csng-n
SUB Main()
  IF CDBL("12.5") = 12.5 THEN PRINT "cdbl-s"
  IF CSNG("2.5") = 2.5 THEN PRINT "csng-s"
  IF CDBL("-4") = -4 THEN PRINT "cdbl-neg"
  ' Ramo numerico: identity, nessuna regressione introdotta dalla coercizione.
  IF CDBL(7) = 7 THEN PRINT "cdbl-n"
  IF CSNG(3) = 3 THEN PRINT "csng-n"
END SUB
