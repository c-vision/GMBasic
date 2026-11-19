' Stesso nome usato come iteratore di un FOR EACH su DICT (elemento boxed, val) e poi di un
' FOR EACH su LISTA (elemento stringa). Sul C99 il ramo lista assegnava gmb_string_t a una
' variabile gmb_val_t: il programma non compilava (e le stringhe stampate perdevano testo).
' Ora il ramo lista assegna gmb_make_val_string(...) e le operazioni stringa usano il testo.
' EXPECT: dictn= 2|list=xyz
IMPORT JSON
SUB Main()
  DIM j AS JSON
  j = JSON.PARSE("{" + CHR$(34) + "a" + CHR$(34) + ":1," + CHR$(34) + "b" + CHR$(34) + ":2}")
  DIM e AS STRING
  DIM n AS LONG
  n = 0
  FOR EACH e IN j
    n = n + 1
  NEXT
  PRINT "dictn="; n

  DIM parts() AS STRING
  parts = SPLIT("x,y,z", ",")
  DIM acc AS STRING
  acc = ""
  FOR EACH e IN parts
    acc = acc + e
  NEXT
  PRINT "list="; acc
END SUB
