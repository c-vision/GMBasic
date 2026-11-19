' CONTINUE FOR / WHILE / DO nella forma a KEYWORD (senza etichetta).
' Il tipo deve coincidere col ciclo piu' interno; l'emitter C produce `continue;`.
' Prima su C99Native era `goto lbl_FOR_CONTINUE` verso un'etichetta mai emessa (C invalido).
' EXPECT: f= 9|w= 12|d= 4
SUB Main()
  DIM i AS LONG
  DIM acc AS LONG
  acc = 0
  FOR i = 1 TO 6
    IF i / 2 = INT(i / 2) THEN CONTINUE FOR
    acc = acc + i
  NEXT
  PRINT "f="; acc

  acc = 0 : i = 0
  WHILE i < 5
    i = i + 1
    IF i = 3 THEN CONTINUE WHILE
    acc = acc + i
  WEND
  PRINT "w="; acc

  acc = 0 : i = 0
  DO
    i = i + 1
    IF i = 2 THEN CONTINUE DO
    acc = acc + i
  LOOP WHILE i < 3
  PRINT "d="; acc
END SUB
