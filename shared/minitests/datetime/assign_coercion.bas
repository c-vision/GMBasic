' MiniTest: coercizione del valore all'ASSEGNAZIONE in base al tipo della variabile.
' Canone IL: data → STRINGA = testo "dd/MM/yyyy HH:mm:ss"; data → NUMERICO = seriale OADate
' (e arrotondamento all'intero con pareggio al PARI per BYTE/INTEGER/LONG: 2.5→2, 3.5→4).
' Prima PCode/VM-C conservavano l'oggetto data nella variabile numerica/stringa e il C99
' non arrotondava (2.5 restava 2.5).
' EXPECT: s=02/01/2020 00:00:00|n= 43832|f= 43832|d=02/01/2020 00:00:00|r25= 2|r35= 4|r19= 2
SUB Main()
  DIM s AS STRING
  DIM n AS LONG
  DIM f AS DOUBLE
  DIM d AS DATETIME
  DIM r1 AS INTEGER
  DIM r2 AS LONG
  DIM r3 AS LONG
  s = DATEVALUE("2020-01-02")
  n = DATEVALUE("2020-01-02")
  f = DATEVALUE("2020-01-02")
  d = DATEVALUE("2020-01-02")
  r1 = 2.5
  r2 = 3.5
  r3 = 1.9
  PRINT "s="; s
  PRINT "n="; n
  PRINT "f="; f
  PRINT "d="; d
  PRINT "r25="; r1
  PRINT "r35="; r2
  PRINT "r19="; r3
END SUB
