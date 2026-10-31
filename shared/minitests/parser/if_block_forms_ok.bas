' Controprova: le forme VALIDE di IF continuano a funzionare (blocco, riga singola con ELSE, ELSEIF, annidato) — il nuovo errore
' sui terminatori orfani non deve toccarle.
' EXPECT: a= 2|b= 0|c= 3|d= 7|e= 10
SUB Main()
  DIM lf AS LONG
  DIM r AS LONG
  DIM i AS LONG
  lf = 0
  IF lf = 0 THEN
    r = 2
  ELSE
    r = 0
  END IF
  PRINT "a="; r
  IF lf = 1 THEN r = 1 ELSE r = 0
  PRINT "b="; r
  IF lf = 5 THEN
    r = 1
  ELSEIF lf = 0 THEN
    r = 3
  ELSE
    r = 4
  END IF
  PRINT "c="; r
  FOR i = 1 TO 2
    IF i = 2 THEN
      IF lf = 0 THEN
        r = 7
      END IF
    END IF
  NEXT
  PRINT "d="; r
  SELECT CASE r
    CASE 7
      r = 10
    CASE ELSE
      r = 0
  END SELECT
  PRINT "e="; r
END SUB
