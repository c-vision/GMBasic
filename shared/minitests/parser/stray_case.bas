' CASE senza SELECT CASE aperto: errore di parsing (prima HANG del front-end).
' EXPECT: COMPILE ERROR: CASE without a matching SELECT CASE.
SUB Main()
  r = 1
  CASE 1
  PRINT r
END SUB
