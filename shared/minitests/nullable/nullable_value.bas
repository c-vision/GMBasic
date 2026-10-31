' FILE: minitests/nullable/nullable_value.bas
' `x.VALUE` — accesso al valore sottostante (numerico e stringa).
' EXPECT: d= 3.14|d-ok|i= 100|s=Hello Nullable
SUB Main()
  DIM d AS DOUBLE?
  d = 3.14
  PRINT "d="; d.VALUE
  IF ABS(d.VALUE - 3.14) < 0.001 THEN PRINT "d-ok"
  DIM i AS INTEGER? = 100
  PRINT "i="; i.VALUE
  DIM s AS STRING?
  s = "Hello Nullable"
  PRINT "s="; s.VALUE
END SUB
