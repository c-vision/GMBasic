' Variabile IMPLICITA (mai dichiarata) assegnata per la prima volta dentro un blocco annidato (IF / ELSE / FOR / WHILE / SELECT CASE):
' in BASIC vive per tutta la procedura, quindi e' visibile dopo il blocco. Prima divergevano: IL e C99Native leggevano il valore,
' PCode / VM-C leggevano 0 (la variabile nasceva nello scope del blocco).
' EXPECT: a= 2|b= 5|c= 3|d= 7|e= 9|f= 4
SUB Main()
  IF 1 = 1 THEN
    a = 2
  END IF
  PRINT "a="; a
  IF 1 = 2 THEN
    b = 1
  ELSE
    b = 5
  END IF
  PRINT "b="; b
  FOR i = 1 TO 3
    c = i
  NEXT
  PRINT "c="; c
  k = 0
  WHILE k < 2
    k = k + 1
    d = k + 5
  WEND
  PRINT "d="; d
  SELECT CASE k
    CASE 2
      e = 9
    CASE ELSE
      e = 0
  END SELECT
  PRINT "e="; e
  IF 1 = 1 THEN
    IF 2 = 2 THEN
      f = 4
    END IF
  END IF
  PRINT "f="; f
END SUB
