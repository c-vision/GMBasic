' EXPECT: time-serial-ok|date-serial-ok|strtime-fmt-ok|strdate-fmt-ok|alias-ok|ti-counter-ok|year-ok
' Canone data/ora (UTC) sui 4 backend:
'   TIME()              -> seriale data+ora numerico (es. 46302.9034027778)
'   DATE()              -> seriale del solo giorno (es. 46302)
'   STRTIME()/TIME$/TI$ -> "HH:MM:SS"
'   STRDATE()/DATE$     -> "dd/MM/yyyy"
'   TI()                -> contatore (ms), resta numerico
SUB Main()
  DIM t AS DOUBLE = TIME()
  DIM d AS DOUBLE = DATE()
  IF t > 46000 AND t < 100000 THEN PRINT "time-serial-ok"
  IF d = INT(d) AND d > 46000 AND d <= t + 1 THEN PRINT "date-serial-ok"
  DIM st AS STRING = STRTIME()
  IF LEN(st) = 8 AND MID$(st, 3, 1) = ":" AND MID$(st, 6, 1) = ":" THEN PRINT "strtime-fmt-ok"
  DIM sd AS STRING = STRDATE()
  IF LEN(sd) = 10 AND MID$(sd, 3, 1) = "/" AND MID$(sd, 6, 1) = "/" THEN PRINT "strdate-fmt-ok"
  DIM a1 AS STRING = TIME$
  DIM a2 AS STRING = TI$
  DIM a3 AS STRING = DATE$()
  IF LEN(a1) = 8 AND LEN(a2) = 8 AND LEN(a3) = 10 AND MID$(a1, 3, 1) = ":" THEN PRINT "alias-ok"
  IF TI() >= 0 THEN PRINT "ti-counter-ok"
  IF VAL(RIGHT$(sd, 4)) >= 2026 THEN PRINT "year-ok"
END SUB
