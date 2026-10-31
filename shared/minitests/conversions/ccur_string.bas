' MiniTest CCUR da STRINGA numerica (4 backend): parse invariant, come VAL.
' Asse diverso dal canone bool (conversions/bool_numeric.bas): qui la sorgente è testo.
' EXPECT: ccur-s|ccur-neg|ccur-val
SUB Main()
  IF CCUR("12.5") = 12.5 THEN PRINT "ccur-s"
  IF CCUR("-3") = -3 THEN PRINT "ccur-neg"
  ' Controllo incrociato: CCUR e VAL devono leggere lo stesso prefisso numerico.
  IF CCUR("7") = VAL("7") THEN PRINT "ccur-val"
END SUB
