' FOR EACH v IN ch (parita' Go for v := range ch): itera finche' chiuso.
IMPORT CONCURRENCY

SUB Main()
  DIM ch AS CHANNEL OF INTEGER (3)
  DIM v AS INTEGER
  DIM Tot AS LONG = 0
  SEND 1 TO ch
  SEND 2 TO ch
  SEND 3 TO ch
  CHANNEL CLOSE ch
  FOR EACH v IN ch
    Tot = Tot + v
  NEXT
  IF Tot = 6 THEN PRINT "FOR-EACH-CH-OK" ELSE PRINT "FAIL:Tot="; Tot END IF
END SUB
